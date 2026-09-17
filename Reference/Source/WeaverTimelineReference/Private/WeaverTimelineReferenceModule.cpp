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

class FWeaverTimelineReferenceModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("WeaverTimelineReference"),
            FOnSpawnTab::CreateRaw(this, &FWeaverTimelineReferenceModule::SpawnTab))
            .SetDisplayName(FText::FromString(TEXT("Weaver 编排参考")));
    }
    virtual void ShutdownModule() override
    {
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("WeaverTimelineReference"));
        if (auto Tab = OpenTab.Pin()) { Tab->RequestCloseTab(); }
    }
private:
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
};

IMPLEMENT_MODULE(FWeaverTimelineReferenceModule, WeaverTimelineReference)
