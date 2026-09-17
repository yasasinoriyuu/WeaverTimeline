#include "Misc/AutomationTest.h"
#include "SWeaverEditableTimeline.h"
#include "WeaverTimelineReferenceAdapter.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWindow.h"
#include "InputCoreTypes.h"
#include "ISequencerModule.h"
#include "LevelEditorSequencerIntegration.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "ViewRangeInterpolation.h"
#include "AnimatedRange.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr auto BoundaryFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

// Actual widget ancestry, Slate routing and capture; no calls to the business Commit method.
struct FWeaverPointerWindow
{
    TSharedRef<SWeaverEditableTimeline> Editable;
    TSharedRef<SWindow> Window;
    FGeometry Geometry;
    FWidgetPath Path;
    explicit FWeaverPointerWindow(TSharedRef<SWeaverEditableTimeline> InEditable)
        : Editable(InEditable), Window(SNew(SWindow).ClientSize(FVector2D(1120, 240))[InEditable])
    {
        FSlateApplication::Get().AddWindow(Window, false);
        Window->SlatePrepass();
        FWidgetPath LayoutPath;
        FSlateApplication::Get().GeneratePathToWidgetChecked(Editable->GetTimeline(), LayoutPath, EVisibility::All);
        Geometry = LayoutPath.Widgets.Last().Geometry;
        TArray<FWidgetAndPointer> Nodes;
        for (int32 Index = 0; Index < LayoutPath.Widgets.Num(); ++Index) { Nodes.Add(FWidgetAndPointer(LayoutPath.Widgets[Index])); }
        Path = FWidgetPath(MakeArrayView(Nodes));
    }
    ~FWeaverPointerWindow()
    {
        Editable->Deactivate();
        FSlateApplication::Get().RequestDestroyWindow(Window);
    }
    FVector2D At(double Frame, double Y = 42) const
    {
        return Geometry.LocalToAbsolute(FVector2D(148 + Frame * (Geometry.GetLocalSize().X - 160) / 240, Y));
    }
    FPointerEvent Pointer(FVector2D Current, FVector2D Previous, FKey Button, bool Down, float Wheel = 0) const
    {
        TSet<FKey> Buttons;
        if (Down) { Buttons.Add(Button); }
        return FPointerEvent(0, Current, Previous, Buttons, Button, Wheel, FModifierKeysState());
    }
    FReply Down(double Frame, FKey Button = EKeys::LeftMouseButton)
    {
        return FSlateApplication::Get().RoutePointerDownEvent(Path, Pointer(At(Frame), At(Frame), Button, true));
    }
    void Up(double Frame, FKey Button = EKeys::LeftMouseButton)
    {
        FSlateApplication::Get().RoutePointerUpEvent(Path, Pointer(At(Frame), At(Frame), Button, false));
    }
    void Move(double From, double To, FKey Button)
    {
        FSlateApplication::Get().RoutePointerMoveEvent(Path, Pointer(At(To), At(From), Button, true), false);
    }
};

struct FPreparationAdapter : FWeaverTimelineReferenceAdapter
{
    TFunction<void()> DuringPreparation;
    int32 TransactionFinishes = 0;
    EWeaverEditOutcome TransactionOutcome = EWeaverEditOutcome::Applied;
    EWeaverEditEndReason EndReason = EWeaverEditEndReason::Finished;
    virtual TUniquePtr<IWeaverEditTransaction> BeginTransaction(const FWeaverEditSession& Session) override
    {
        struct FObservedTransaction : IWeaverEditTransaction
        {
            TUniquePtr<IWeaverEditTransaction> Inner;
            FPreparationAdapter& Owner;
            FObservedTransaction(TUniquePtr<IWeaverEditTransaction> InInner, FPreparationAdapter& InOwner)
                : Inner(MoveTemp(InInner)), Owner(InOwner) {}
            virtual void Finish(EWeaverEditOutcome Outcome) override
            {
                ++Owner.TransactionFinishes;
                Owner.TransactionOutcome = Outcome;
                Inner->Finish(Outcome);
            }
        };
        auto Scope = FWeaverTimelineReferenceAdapter::BeginTransaction(Session);
        DuringPreparation();
        return MakeUnique<FObservedTransaction>(MoveTemp(Scope), *this);
    }
    virtual void EndPreview(const FWeaverEditSession& Session, const FWeaverEditResult& Result, EWeaverEditEndReason Reason) override
    {
        EndReason = Reason;
        FWeaverTimelineReferenceAdapter::EndPreview(Session, Result, Reason);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverPrepareCancelTest, "WeaverTimeline.Boundaries.CancelBeforeCommit", BoundaryFlags)
bool FWeaverPrepareCancelTest::RunTest(const FString&)
{
    for (int32 Mode = 0; Mode < 4; ++Mode)
    {
        auto Adapter = MakeShared<FPreparationAdapter>();
        auto Widget = SNew(SWeaverEditableTimeline).Adapter(Adapter);
        auto Controller = Widget->GetController();
        Adapter->DuringPreparation = [&, Mode]()
        {
            if (Mode == 0) { Controller->CancelEdit(); }
            if (Mode == 1) { Widget->Deactivate(); }
            if (Mode == 2) { Controller->DetachTimeline(); }
            if (Mode == 3) { Adapter->NotifySourceChanged(); }
        };
        FWeaverEditProposal P;
        P.LaneId = Adapter->GetDocument().LaneId;
        P.ItemId = Adapter->GetDocument().Items[0].Id;
        Controller->BeginEdit(P);
        Controller->FinishEdit(50, 90, false);
        TestEqual(TEXT("Cancellation during transaction preparation prevents Commit"), Adapter->CommitCount, 0);
        TestEqual(TEXT("Authority unchanged"), Adapter->GetDocument().Items[0].Start, 20.0);
        TestEqual(TEXT("Transaction finished once"), Adapter->TransactionFinishes, 1);
        TestTrue(TEXT("Transaction cancelled"), Adapter->TransactionOutcome == EWeaverEditOutcome::Cancelled);
        TestEqual(TEXT("Preview ended once"), Adapter->EndPreviewCount, 1);
        TestEqual(TEXT("No preview remains"), Adapter->Previews.Num(), 0);
        const EWeaverEditEndReason Expected[] = { EWeaverEditEndReason::UserCancelled, EWeaverEditEndReason::Hidden,
            EWeaverEditEndReason::Detached, EWeaverEditEndReason::SourceChanged };
        TestTrue(TEXT("Cancellation reason survives preparation"), Adapter->EndReason == Expected[Mode]);
        Controller->FinishEdit(50, 90, false);
        TestEqual(TEXT("Late finish does not commit"), Adapter->CommitCount, 0);
        Adapter->DuringPreparation = nullptr;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverScrubCancelTest, "WeaverTimeline.Boundaries.ScrubCancellation", BoundaryFlags)
bool FWeaverScrubCancelTest::RunTest(const FString&)
{
    for (int32 Mode = 0; Mode < 2; ++Mode)
    {
        auto Adapter = MakeShared<FWeaverTimelineReferenceAdapter>();
        TWeakPtr<SWeaverEditableTimeline> Weak;
        auto Widget = SNew(SWeaverEditableTimeline).Adapter(Adapter)
            .OnFrameChanged_Lambda([&](double)
            {
                if (Mode == 0) { Weak.Pin()->Deactivate(); }
                else { Adapter->ReplaceDocument(); }
            });
        Weak = Widget;
        FWeaverPointerWindow Input(Widget);
        const auto Reply = Input.Down(180);
        TestFalse(TEXT("Cancelled scrub reply does not request capture"), Reply.GetMouseCaptor().IsValid());
        TestFalse(TEXT("Cancelled scrub has no Slate mouse capture"), Widget->GetTimeline()->HasMouseCapture());
        Input.Up(180);
        TestEqual(TEXT("Scrub cancellation never commits data"), Adapter->CommitCount, 0);
    }
    // Selection callbacks run before drag state is established; cancellation must survive that too.
    auto Adapter = MakeShared<FWeaverTimelineReferenceAdapter>();
    TWeakPtr<SWeaverEditableTimeline> Weak;
    auto Widget = SNew(SWeaverEditableTimeline).Adapter(Adapter)
        .OnSelectionChanged_Lambda([&](FWeaverSelection) { Weak.Pin()->Deactivate(); });
    Weak = Widget;
    FWeaverPointerWindow Input(Widget);
    for (const FKey Button : { EKeys::LeftMouseButton, EKeys::RightMouseButton })
    {
        Widget->GetTimeline()->ClearSelection();
        const auto Reply = Input.Down(40, Button);
        TestFalse(TEXT("Selection cancellation cannot restart left drag or right pan"), Reply.GetMouseCaptor().IsValid());
        Input.Up(40, Button);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverEndpointTest, "WeaverTimeline.Boundaries.EndpointNotification", BoundaryFlags)
bool FWeaverEndpointTest::RunTest(const FString&)
{
    for (bool Jump : { true, false })
    {
        auto Adapter = MakeShared<FWeaverTimelineReferenceAdapter>();
        int32 Clicks = 0;
        FGuid Lane, Block;
        bool Start = false;
        auto Widget = SNew(SWeaverEditableTimeline).Adapter(Adapter).JumpToEndpointOnClick(Jump)
            .OnBlockEndpointClicked_Lambda([&](FGuid InLane, FGuid InBlock, bool InStart)
            { ++Clicks; Lane = InLane; Block = InBlock; Start = InStart; });
        Widget->SetCurrentFrame(110);
        FWeaverPointerWindow Input(Widget);
        Input.Down(20.1); Input.Up(20.1);
        TestEqual(TEXT("Start endpoint emits one event"), Clicks, 1);
        TestEqual(TEXT("Lane identity forwarded"), Lane, Adapter->GetDocument().LaneId);
        TestEqual(TEXT("Block identity forwarded"), Block, Adapter->GetDocument().Items[0].Id);
        TestTrue(TEXT("Start endpoint identified"), Start);
        TestEqual(TEXT("Default jump can be disabled"), Widget->GetCurrentFrame(), Jump ? 20.0 : 110.0);
        Input.Down(59.9); Input.Up(59.9);
        TestEqual(TEXT("End endpoint emits one event"), Clicks, 2);
        TestFalse(TEXT("End endpoint identified"), Start);
        TestEqual(TEXT("End jump follows policy"), Widget->GetCurrentFrame(), Jump ? 60.0 : 110.0);
        Input.Down(180); Input.Up(180);
        TestEqual(TEXT("Ordinary scrub cannot masquerade as endpoint click"), Clicks, 2);
        TestEqual(TEXT("Endpoint click does not commit a resize"), Adapter->CommitCount, 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverSequencerOptInTest, "WeaverTimeline.Boundaries.SequencerOptIn", BoundaryFlags)
bool FWeaverSequencerOptInTest::RunTest(const FString&)
{
    auto& Integration = FLevelEditorSequencerIntegration::Get();
    if (Integration.GetSequencers().Num() != 0)
    {
        AddError(TEXT("Run this isolation test without user Sequencers open."));
        return false;
    }
    TStrongObjectPtr<ULevelSequence> Sequence(NewObject<ULevelSequence>(GetTransientPackage(), NAME_None, RF_Transactional));
    Sequence->Initialize();
    Sequence->GetMovieScene()->SetDisplayRate(FFrameRate(30, 1));
    FSequencerInitParams Params;
    Params.RootSequence = Sequence.Get();
    Params.ViewParams.UniqueName = TEXT("WeaverOptInTest");
    auto Sequencer = FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer")).CreateSequencer(Params);
    FLevelEditorSequencerIntegrationOptions Options;
    Options.bRequiresLevelEvents = Options.bRequiresActorEvents = Options.bForceRefreshDetails = false;
    Options.bAttachOutlinerColumns = Options.bActivateSequencerEdMode = Options.bSyncBindingsToActorLabels = false;
    Integration.AddSequencer(Sequencer, Options);
    Sequencer->SetLocalTimeDirectly(FFrameTime(24000), true);
    Sequencer->SetViewRange(TRange<double>(0, 8), EViewRangeInterpolation::Immediate);
    const auto InitialTime = Sequencer->GetLocalTime().Time;
    const auto InitialRange = Sequencer->GetViewRange();
    {
        auto Adapter = MakeShared<FWeaverTimelineReferenceAdapter>();
        auto Widget = SNew(SWeaverEditableTimeline).Adapter(Adapter).SyncSequencer(false);
        FWeaverPointerWindow Input(Widget);
        for (int32 Index = 0; Index < 3; ++Index) { Widget->Tick(Input.Geometry, Index, 0.016f); }
        Input.Down(180); Input.Move(180, 190, EKeys::LeftMouseButton); Input.Up(190);
        Input.Down(180, EKeys::RightMouseButton); Input.Move(180, 195, EKeys::RightMouseButton); Input.Up(195, EKeys::RightMouseButton);
        Widget->GetTimeline()->OnMouseWheel(Input.Geometry, Input.Pointer(Input.At(180), Input.At(180), FKey(), false, 1));
        TestTrue(TEXT("Disabled editable timeline leaves real Sequencer time untouched"), Sequencer->GetLocalTime().Time == InitialTime);
        TestEqual(TEXT("Disabled pan/zoom leaves lower bound"), Sequencer->GetViewRange().GetLowerBoundValue(), InitialRange.GetLowerBoundValue());
        TestEqual(TEXT("Disabled pan/zoom leaves upper bound"), Sequencer->GetViewRange().GetUpperBoundValue(), InitialRange.GetUpperBoundValue());

        FWeaverSequencerBridge Bridge;
        double Received = -1;
        const auto Connect = [&]()
        {
            Bridge.Register(Widget->GetTimeline(), [] { return FFrameRate(30, 1); },
                FOnWeaverSequencerFrameChanged::CreateLambda([&](double Frame) { Received = Frame; }));
        };
        Bridge.Sync(Input.Geometry);
        TestFalse(TEXT("Unregistered bridge never discovers a Sequencer"), Bridge.IsBound());
        Connect();
        TestTrue(TEXT("Explicit registration binds real Sequencer"), Bridge.IsBound());
        Bridge.PushTimelineFrame(90);
        TestEqual(TEXT("Enabled forward time sync"), Sequencer->GetLocalTime().AsSeconds(), 3.0);
        Sequencer->SetLocalTimeDirectly(FFrameRate::TransformTime(FFrameTime(120), FFrameRate(30, 1), Sequencer->GetFocusedTickResolution()), true);
        TestEqual(TEXT("Enabled reverse time sync"), Received, 120.0);
        Bridge.PushTimelineViewRange(30, 150);
        TestEqual(TEXT("Enabled view sync"), Sequencer->GetViewRange().GetLowerBoundValue(), 1.0);
        Bridge.Unregister();
        const double Before = Received;
        Bridge.Sync(Input.Geometry);
        Bridge.PushTimelineFrame(210);
        Bridge.PushTimelineViewRange(0, 240);
        TestFalse(TEXT("Unregister prevents reattachment via Sync"), Bridge.IsBound());
        TestEqual(TEXT("Unregistered bridge cannot change time"), Sequencer->GetLocalTime().AsSeconds(), 4.0);
        TestEqual(TEXT("Unregistered bridge cannot change range"), Sequencer->GetViewRange().GetLowerBoundValue(), 1.0);
        Sequencer->SetLocalTimeDirectly(FFrameTime(0), true);
        TestEqual(TEXT("Unregister removes reverse callback"), Received, Before);
        Connect();
        Bridge.PushTimelineFrame(60);
        TestEqual(TEXT("Re-register restores sync"), Sequencer->GetLocalTime().AsSeconds(), 2.0);
        Bridge.Unregister();
    }
    Integration.RemoveSequencer(Sequencer);
    Sequencer->Close();
    return true;
}
#endif
