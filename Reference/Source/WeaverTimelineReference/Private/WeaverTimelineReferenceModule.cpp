#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Editor.h"
#include "SWeaverEditableTimeline.h"
#include "WeaverTimelineReferenceAdapter.h"
#include "ISequencerModule.h"
#include "WeaverReferenceTrackEditor.h"
#include "WeaverReferenceTrack.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ViewRangeInterpolation.h"
#include "Misc/CoreDelegates.h"
#include "SequencerSettings.h"

class FWeaverTimelineReferenceModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        TrackEditorHandle = FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"))
            .RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FWeaverReferenceTrackEditor::Create));
        DemoCommand = MakeUnique<FAutoConsoleCommand>(TEXT("Weaver.ReferenceEmbedded"),
            TEXT("Open a transient native Sequencer containing the embedded reference root track."),
            FConsoleCommandDelegate::CreateRaw(this, &FWeaverTimelineReferenceModule::OpenEmbeddedDemo));
        PreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FWeaverTimelineReferenceModule::CloseEmbeddedDemo);
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("WeaverTimelineReference"),
            FOnSpawnTab::CreateRaw(this, &FWeaverTimelineReferenceModule::SpawnTab))
            .SetDisplayName(FText::FromString(TEXT("Weaver 编排参考")));
    }
    virtual void ShutdownModule() override
    {
        DemoCommand.Reset();
        FCoreDelegates::OnEnginePreExit.Remove(PreExitHandle);
        CloseEmbeddedDemo();
        if (auto* Module = FModuleManager::GetModulePtr<ISequencerModule>(TEXT("Sequencer")))
        { Module->UnRegisterTrackEditor(TrackEditorHandle); }
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("WeaverTimelineReference"));
        if (auto Tab = OpenTab.Pin()) { Tab->RequestCloseTab(); }
    }
private:
    void ReleaseEmbeddedDemo()
    {
        if (DemoSequencer)
        {
            auto Owner = MoveTemp(DemoSequencer);
            Owner->Close();
        }
        DemoSequence.Reset();
        DemoWindow.Reset();
    }
    void CloseEmbeddedDemo()
    {
        auto Window = DemoWindow.Pin();
        if (Window) { Window->SetOnWindowClosed(FOnWindowClosed()); }
        ReleaseEmbeddedDemo();
        if (Window) { Window->RequestDestroyWindow(); }
    }
    void OpenEmbeddedDemo()
    {
        if (auto Window = DemoWindow.Pin()) { Window->BringToFront(); return; }
        if (DemoSequencer) { DemoSequencer->Close(); DemoSequencer.Reset(); }
        DemoSequence.Reset(NewObject<ULevelSequence>(GetTransientPackage(), NAME_None, RF_Transactional));
        DemoSequence->Initialize();
        auto* Scene = DemoSequence->GetMovieScene();
        Scene->SetDisplayRate(FFrameRate(30, 1));
        Scene->SetPlaybackRange(0, 192000);
        auto* Track = Scene->AddTrack<UWeaverReferenceTrack>();
        Track->AddSection(*Track->CreateNewSection());
        FSequencerInitParams Params;
        Params.RootSequence = DemoSequence.Get();
        Params.ViewParams.UniqueName = TEXT("WeaverV7EmbeddedDemo");
        // SSequencer reads its initial outliner width during construction.
        auto* Settings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(*Params.ViewParams.UniqueName);
        Settings->SetTreeViewWidth(400.f);
        Settings->SetTimeDisplayFormat(EFrameNumberDisplayFormats::Frames);
        DemoSequencer = FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer")).CreateSequencer(Params);
        DemoSequencer->SetViewRange(TRange<double>(0, 8), EViewRangeInterpolation::Immediate);
        auto Window = SNew(SWindow).Title(FText::FromString(TEXT("Weaver V7 · Sequencer 顶层嵌入验证")))
            .ClientSize(FVector2D(1200, 550))[DemoSequencer->GetSequencerWidget()];
        Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>&)
        { ReleaseEmbeddedDemo(); }));
        DemoWindow = Window;
        FSlateApplication::Get().AddWindow(Window);
    }
    TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
    {
        auto Adapter = MakeShared<FWeaverTimelineReferenceAdapter>();
        TSharedRef<SWeaverEditableTimeline> Timeline = SNew(SWeaverEditableTimeline).Adapter(Adapter);
        auto Tab = SNew(SDockTab).TabRole(ETabRole::NomadTab)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(8)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("拖动片段、边缘或关键帧；展开片段编辑生效区间。Esc 取消，Delete 删除。数值从权威对象回读。")))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(8)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .Text_Lambda([Adapter]()
                    {
                        const TCHAR* Names[] = { TEXT("普通"), TEXT("吸附到十帧"), TEXT("限制范围"), TEXT("拒绝提交"), TEXT("级联推后") };
                        return FText::FromString(FString::Printf(TEXT("策略：%s（点击切换）"), Names[int32(Adapter->Policy)]));
                    })
                    .OnClicked_Lambda([Adapter]()
                    {
                        Adapter->Policy = EWeaverReferencePolicy((int32(Adapter->Policy) + 1) % 5);
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
                [ SNew(SButton).Text(FText::FromString(TEXT("撤销"))).OnClicked_Lambda([]() { GEditor->UndoTransaction(); return FReply::Handled(); }) ]
                + SHorizontalBox::Slot().AutoWidth()
                [ SNew(SButton).Text(FText::FromString(TEXT("重做"))).OnClicked_Lambda([]() { GEditor->RedoTransaction(); return FReply::Handled(); }) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
                [ SNew(SButton).Text(FText::FromString(TEXT("切换数据源"))).OnClicked_Lambda([Adapter]() { Adapter->ReplaceDocument(); return FReply::Handled(); }) ]
            ]
            + SVerticalBox::Slot().FillHeight(1).Padding(8)
            [ Timeline ]
            + SVerticalBox::Slot().AutoHeight().Padding(8)
            [
                SNew(STextBlock).Text_Lambda([Adapter]()
                {
                    FString Text = FString::Printf(TEXT("活动预览：%d　预览开始/结束：%d/%d\n权威数据："),
                        Adapter->Previews.Num(), Adapter->BeginPreviewCount, Adapter->EndPreviewCount);
                    for (const auto& Item : Adapter->GetDocument().Items)
                    {
                        Text += FString::Printf(TEXT(" %s %.2f～%.2f；"), Item.bKey ? TEXT("关键帧") : TEXT("片段"), Item.Start, Item.End);
                    }
                    return FText::FromString(Text);
                })
            ]
        ];
        Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([Weak = TWeakPtr<SWeaverEditableTimeline>(Timeline)](TSharedRef<SDockTab>)
        {
            if (auto Widget = Weak.Pin()) { Widget->Deactivate(); }
        }));
        OpenTab = Tab;
        return Tab;
    }
    TWeakPtr<SDockTab> OpenTab;
    FDelegateHandle TrackEditorHandle;
    FDelegateHandle PreExitHandle;
    TUniquePtr<FAutoConsoleCommand> DemoCommand;
    TStrongObjectPtr<ULevelSequence> DemoSequence;
    TSharedPtr<ISequencer> DemoSequencer;
    TWeakPtr<SWindow> DemoWindow;
};

IMPLEMENT_MODULE(FWeaverTimelineReferenceModule, WeaverTimelineReference)
