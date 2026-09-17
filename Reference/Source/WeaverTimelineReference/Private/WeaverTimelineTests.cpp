#include "Misc/AutomationTest.h"
#include "SWeaverEditableTimeline.h"
#include "WeaverTimelineReferenceAdapter.h"
#include "Editor.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWindow.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FWeaverFixture
{
    TSharedRef<FWeaverTimelineReferenceAdapter> Adapter = MakeShared<FWeaverTimelineReferenceAdapter>();
    TSharedRef<SWeaverEditableTimeline> Widget = SNew(SWeaverEditableTimeline).Adapter(Adapter);
    TSharedRef<FWeaverTimelineEditController> Controller = Widget->GetController();
    FWeaverEditProposal Block(EWeaverBlockEditKind Kind = EWeaverBlockEditKind::Move) const
    {
        FWeaverEditProposal P;
        P.LaneId = Adapter->GetDocument().LaneId;
        P.ItemId = Adapter->GetDocument().Items[0].Id;
        P.BlockKind = Kind;
        return P;
    }
};
constexpr auto WeaverTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverAuthorityTest, "WeaverTimeline.Lifecycle.AuthorityWins", WeaverTestFlags)
bool FWeaverAuthorityTest::RunTest(const FString&)
{
    for (auto Policy : { EWeaverReferencePolicy::Normal, EWeaverReferencePolicy::Snap, EWeaverReferencePolicy::Clamp, EWeaverReferencePolicy::Reject })
    {
        FWeaverFixture F;
        F.Adapter->Policy = Policy;
        const FGuid Id = F.Block().ItemId;
        TestTrue(TEXT("Begin"), F.Controller->BeginEdit(F.Block()));
        F.Controller->UpdateEdit(53, 93);
        TestEqual(TEXT("Preview leaves authority untouched"), F.Adapter->GetDocument().Items[0].Start, 20.0);
        F.Controller->FinishEdit(53, 93, false);
        const double Expected = Policy == EWeaverReferencePolicy::Snap ? 50 : Policy == EWeaverReferencePolicy::Reject ? 20 : 53;
        TestEqual(TEXT("Authoritative commit policy"), F.Adapter->GetDocument().Items[0].Start, Expected);
        TestEqual(TEXT("Widget reads authority after finish"), F.Widget->GetTimeline()->GetDisplayedBlocks()[0].StartFrame, Expected);
        TestEqual(TEXT("Stable ID"), F.Controller->GetPresentation().Blocks[0].BlockId, Id);
        TestEqual(TEXT("Preview ended once"), F.Adapter->EndPreviewCount, 1);
        TestEqual(TEXT("No external preview remains"), F.Adapter->Previews.Num(), 0);
        F.Controller->RefreshFromSource();
        auto Recreated = SNew(SWeaverEditableTimeline).Adapter(F.Adapter);
        TestEqual(TEXT("Recreated widget reads authority"), Recreated->GetTimeline()->GetDisplayedBlocks()[0].StartFrame, Expected);
    }
    FWeaverFixture Clamp;
    Clamp.Adapter->Policy = EWeaverReferencePolicy::Clamp;
    Clamp.Controller->BeginEdit(Clamp.Block());
    Clamp.Controller->FinishEdit(-20, 20, false);
    TestEqual(TEXT("Clamped proposal is legitimate"), Clamp.Widget->GetTimeline()->GetDisplayedBlocks()[0].StartFrame, 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverEditKindsTest, "WeaverTimeline.Lifecycle.EditKindsCascadeUndo", WeaverTestFlags)
bool FWeaverEditKindsTest::RunTest(const FString&)
{
    FWeaverFixture F;
    const FGuid Id = F.Block().ItemId;
    F.Adapter->Policy = EWeaverReferencePolicy::Cascade;
    F.Controller->BeginEdit(F.Block(EWeaverBlockEditKind::ResizeEnd));
    F.Controller->FinishEdit(20, 85, false);
    TestEqual(TEXT("Resize authority"), F.Adapter->GetDocument().Items[0].End, 85.0);
    TestEqual(TEXT("Cascade authority"), F.Adapter->GetDocument().Items[1].Start, 85.0);
    TestEqual(TEXT("Cascade full presentation refresh"), F.Widget->GetTimeline()->GetDisplayedBlocks()[1].StartFrame, 85.0);
    int32 Notifications = 0;
    F.Controller->OnReconciled.AddLambda([&](const auto&, const auto&) { ++Notifications; });
    const uint64 BeforeUndo = F.Adapter->GetContext().Revision;
    GEditor->UndoTransaction();
    TestEqual(TEXT("Undo authority"), F.Adapter->GetDocument().Items[0].End, 60.0);
    TestEqual(TEXT("Undo full UI"), F.Widget->GetTimeline()->GetDisplayedBlocks()[1].StartFrame, 70.0);
    TestTrue(TEXT("Undo notifies reconciliation"), Notifications > 0);
    TestTrue(TEXT("Undo generation monotonic"), F.Adapter->GetContext().Revision > BeforeUndo);
    GEditor->RedoTransaction();
    TestEqual(TEXT("Redo UI"), F.Widget->GetTimeline()->GetDisplayedBlocks()[1].StartFrame, 85.0);
    TestEqual(TEXT("Undo/Redo stable identity"), F.Adapter->GetDocument().Items[0].Id, Id);
    F.Controller->OnReconciled.Clear();
    F.Adapter->Policy = EWeaverReferencePolicy::Normal;
    F.Controller->BeginEdit(F.Block(EWeaverBlockEditKind::ResizeStart));
    F.Controller->FinishEdit(10, 85, false);
    TestEqual(TEXT("ResizeStart UI"), F.Widget->GetTimeline()->GetDisplayedBlocks()[0].StartFrame, 10.0);
    auto Key = F.Block(); Key.Target = EWeaverEditTarget::Key; Key.ItemType = EWeaverItemType::Key; Key.ItemId = F.Adapter->GetDocument().Items[2].Id;
    F.Controller->BeginEdit(Key); F.Controller->FinishEdit(150, 150, false);
    TestEqual(TEXT("Key authority read"), F.Widget->GetTimeline()->GetDisplayedKeys()[0].Frame, 150.0);
    auto Row = F.Block(); Row.Target = EWeaverEditTarget::TimingRow; Row.RowId = TEXT("Weight");
    F.Controller->BeginEdit(Row); F.Controller->FinishEdit(0.3, 0.7, false);
    TestTrue(TEXT("Timing authority read"), FMath::IsNearlyEqual(F.Widget->GetTimeline()->GetDisplayedBlocks()[0].TimingRows[0].StartRatio, 0.3f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverTerminationTest, "WeaverTimeline.Lifecycle.Termination", WeaverTestFlags)
bool FWeaverTerminationTest::RunTest(const FString&)
{
    for (int32 Mode = 0; Mode < 5; ++Mode)
    {
        FWeaverFixture F;
        F.Controller->BeginEdit(F.Block());
        F.Controller->UpdateEdit(50, 90);
        if (Mode == 0) { F.Controller->CancelEdit(); }
        if (Mode == 1) { F.Controller->DetachTimeline(); }
        if (Mode == 2) { F.Widget->Deactivate(); }
        if (Mode == 3) { F.Adapter->ReplaceDocument(); }
        if (Mode == 4)
        {
            F.Adapter->GetDocument().Items[0].Start = 31;
            F.Adapter->NotifySourceChanged();
        }
        F.Controller->FinishEdit(50, 90, false); // Stale MouseUp may not commit.
        TestFalse(TEXT("Session ended"), F.Controller->HasActiveEdit());
        TestEqual(TEXT("No stale commit"), F.Adapter->CommitCount, 0);
        TestEqual(TEXT("Every terminal path ends preview once"), F.Adapter->EndPreviewCount, 1);
        TestEqual(TEXT("External preview empty"), F.Adapter->Previews.Num(), 0);
        TestEqual(TEXT("Cancel never writes Original over authority"), F.Adapter->GetDocument().Items[0].Start, Mode == 4 ? 31.0 : 20.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverCommandTest, "WeaverTimeline.Lifecycle.CommandsSelectionReentrancy", WeaverTestFlags)
bool FWeaverCommandTest::RunTest(const FString&)
{
    FWeaverFixture F;
    auto Timeline = F.Widget->GetTimeline();
    FWeaverSelection Selected;
    Selected.Type = EWeaverItemType::Block; Selected.LaneId = F.Block().LaneId; Selected.ItemId = F.Block().ItemId;
    Timeline->SetSelection(Selected);
    F.Controller->SetBlockExpanded(Selected.ItemId, true);
    F.Controller->RefreshFromSource();
    TestTrue(TEXT("Selection retained by ID"), Timeline->GetSelection() == Selected);
    TestTrue(TEXT("Expansion survives refresh"), Timeline->GetDisplayedBlocks()[0].bExpanded);
    auto Command = F.Block(); Command.Target = EWeaverEditTarget::Command; Command.CommandId = TEXT("Delete");
    F.Controller->ExecuteCommand(Command);
    TestEqual(TEXT("Delete authority"), F.Adapter->GetDocument().Items.Num(), 2);
    TestEqual(TEXT("Delete refresh automatic"), Timeline->GetDisplayedBlocks().Num(), 1);
    TestFalse(TEXT("Deleted selection cleared"), Timeline->GetSelection().IsValid());
    Command.CommandId = TEXT("Create"); Command.Start = 170;
    F.Controller->ExecuteCommand(Command);
    TestEqual(TEXT("Create refresh automatic"), Timeline->GetDisplayedBlocks().Num(), 2);
    F.Controller->BeginEdit(F.Block());
    F.Adapter->DuringCommit = [Controller = F.Controller]()
    {
        Controller->FinishEdit(99, 139, false);
        Controller->RefreshFromSource();
        Controller->CancelEdit();
    };
    const int32 Before = F.Adapter->CommitCount;
    F.Controller->FinishEdit(45, 75, false);
    F.Adapter->DuringCommit = nullptr;
    TestEqual(TEXT("Reentrant finish cannot double commit"), F.Adapter->CommitCount, Before + 1);
    TestEqual(TEXT("Reentrant preview ends exactly once"), F.Adapter->BeginPreviewCount, F.Adapter->EndPreviewCount);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverPointerTest, "WeaverTimeline.Slate.PointerCommitCancel", WeaverTestFlags)
bool FWeaverPointerTest::RunTest(const FString&)
{
    // Route real Slate pointer events, including capture replies. No direct controller edit calls.
    FWeaverFixture F;
    auto Timeline = F.Widget->GetTimeline();
    auto Window = SNew(SWindow).ClientSize(FVector2D(1120, 240))[ F.Widget ];
    FSlateApplication& App = FSlateApplication::Get();
    App.AddWindow(Window, false);
    Window->SlatePrepass();
    FWidgetPath LayoutPath;
    if (!App.GeneratePathToWidgetUnchecked(Timeline, LayoutPath, EVisibility::All))
    {
        AddError(TEXT("Cannot arrange reference test window"));
        App.RequestDestroyWindow(Window);
        return false;
    }
    const FGeometry Geometry = LayoutPath.Widgets.Last().Geometry;
    // Pointer routing needs virtual-position entries too; the layout-only constructor omits them.
    // Keep every actual ancestor: capture reconstructs this path when subsequent events arrive.
    TArray<FWidgetAndPointer> Arranged;
    for (int32 Index = 0; Index < LayoutPath.Widgets.Num(); ++Index)
    {
        Arranged.Add(FWidgetAndPointer(LayoutPath.Widgets[Index]));
    }
    FWidgetPath Path(MakeArrayView(Arranged));
    auto Pointer = [](FVector2D Position, FVector2D Previous, bool Down, FKey Effect)
    {
        TSet<FKey> Buttons;
        if (Down) { Buttons.Add(EKeys::LeftMouseButton); }
        return FPointerEvent(0, Position, Previous, Buttons, Effect, 0, FModifierKeysState());
    };
    const double PixelsPerFrame = (Geometry.GetLocalSize().X - 160.0) / 240.0;
    const FVector2D Start = Geometry.LocalToAbsolute(FVector2D(148 + 40 * PixelsPerFrame, 42));
    const FVector2D End = Geometry.LocalToAbsolute(FVector2D(148 + 73 * PixelsPerFrame, 42));
    F.Adapter->Policy = EWeaverReferencePolicy::Snap;
    App.RoutePointerDownEvent(Path, Pointer(Start, Start, true, EKeys::LeftMouseButton));
    TestTrue(TEXT("Pointer down creates session"), F.Controller->HasActiveEdit());
    TestTrue(TEXT("Slate capture acquired"), Timeline->HasMouseCapture());
    App.RoutePointerMoveEvent(Path, Pointer(End, Start, true, FKey()), false);
    TestEqual(TEXT("Pointer move only previews"), F.Adapter->GetDocument().Items[0].Start, 20.0);
    TestTrue(TEXT("Session survives routed move"), F.Controller->HasActiveEdit());
    if (F.Adapter->Previews.Num() == 1)
    {
        TestEqual(TEXT("Routed move creates expected proposal"), F.Adapter->Previews.CreateConstIterator().Value().Proposal.Start, 53.0);
    }
    App.RoutePointerUpEvent(Path, Pointer(End, End, false, EKeys::LeftMouseButton));
    TestEqual(TEXT("MouseUp committed resolved authority"), F.Adapter->GetDocument().Items[0].Start, 50.0);
    TestEqual(TEXT("MouseUp final UI is authoritative"), Timeline->GetDisplayedBlocks()[0].StartFrame, 50.0);
    const FVector2D Next = Geometry.LocalToAbsolute(FVector2D(148 + 60 * PixelsPerFrame, 42));
    App.RoutePointerDownEvent(Path, Pointer(Next, Next, true, EKeys::LeftMouseButton));
    App.RoutePointerMoveEvent(Path, Pointer(Next + FVector2D(20, 0), Next, true, FKey()), false);
    const auto EscapeReply = Timeline->OnKeyDown(Geometry, FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
    App.ProcessReply(Path, EscapeReply, nullptr, nullptr);
    TestEqual(TEXT("Esc preserves authority"), F.Adapter->GetDocument().Items[0].Start, 50.0);
    TestFalse(TEXT("Esc clears session"), F.Controller->HasActiveEdit());
    App.RoutePointerDownEvent(Path, Pointer(Next, Next, true, EKeys::LeftMouseButton));
    App.ReleaseAllPointerCapture();
    TestFalse(TEXT("CaptureLost clears session"), F.Controller->HasActiveEdit());
    TestEqual(TEXT("All pointer previews cleaned"), F.Adapter->BeginPreviewCount, F.Adapter->EndPreviewCount);
    F.Adapter->Policy = EWeaverReferencePolicy::Normal;
    F.Adapter->GetDocument().Items.RemoveAt(1); // Isolate resize edges from overlapping B.
    F.Adapter->NotifySourceChanged();
    auto Drag = [&](double FromFrame, double ToFrame, double Y)
    {
        const FVector2D From = Geometry.LocalToAbsolute(FVector2D(148 + FromFrame * PixelsPerFrame, Y));
        const FVector2D To = Geometry.LocalToAbsolute(FVector2D(148 + ToFrame * PixelsPerFrame, Y));
        App.RoutePointerDownEvent(Path, Pointer(From, From, true, EKeys::LeftMouseButton));
        App.RoutePointerMoveEvent(Path, Pointer(To, From, true, FKey()), false);
        App.RoutePointerUpEvent(Path, Pointer(To, To, false, EKeys::LeftMouseButton));
    };
    Drag(50.1, 60.1, 42);
    TestEqual(TEXT("Pointer ResizeStart commits authority"), F.Adapter->GetDocument().Items[0].Start, 60.0);
    Drag(89.9, 99.9, 42);
    TestEqual(TEXT("Pointer ResizeEnd commits authority"), F.Adapter->GetDocument().Items[0].End, 100.0);
    Drag(130, 140, 42);
    TestEqual(TEXT("Pointer Key commits authority"), Timeline->GetDisplayedKeys()[0].Frame, 140.0);
    F.Controller->SetBlockExpanded(F.Block().ItemId, true);
    const int32 BeforeTiming = F.Adapter->CommitCount;
    Drag(68, 72, 69); // 20% -> 30% of [60,100].
    TestEqual(TEXT("Pointer TimingRow reaches commit"), F.Adapter->CommitCount, BeforeTiming + 1);
    TestEqual(TEXT("Pointer TimingRow commits authority"), F.Adapter->GetDocument().Items[0].TimingStart, 0.3f);
    TestEqual(TEXT("All edit kinds clean their previews"), F.Adapter->BeginPreviewCount, F.Adapter->EndPreviewCount);
    App.RequestDestroyWindow(Window);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverBoundaryTest, "WeaverTimeline.Lifecycle.CallbackBoundaries", WeaverTestFlags)
bool FWeaverBoundaryTest::RunTest(const FString&)
{
    FWeaverFixture F;
    F.Controller->BeginEdit(F.Block());
    F.Controller->FinishEdit(20, 60, false);
    TestTrue(TEXT("Identical proposal reports NoChange"), F.Controller->GetLastResult().Outcome == EWeaverEditOutcome::NoChange);
    TestEqual(TEXT("NoChange ends preview"), F.Adapter->EndPreviewCount, 1);
    F.Controller->BeginEdit(F.Block()); F.Controller->FinishEdit(30, 70, false);
    F.Controller->BeginEdit(F.Block()); F.Controller->FinishEdit(30, 70, false);
    F.Adapter->Policy = EWeaverReferencePolicy::Reject;
    F.Controller->BeginEdit(F.Block()); F.Controller->FinishEdit(40, 80, false);
    GEditor->UndoTransaction();
    TestEqual(TEXT("NoChange and Reject do not consume an Undo step"), F.Adapter->GetDocument().Items[0].Start, 20.0);
    FWeaverSelection Selected;
    Selected.Type = EWeaverItemType::Block; Selected.LaneId = F.Block().LaneId; Selected.ItemId = F.Block().ItemId;
    F.Widget->GetTimeline()->SetSelection(Selected);
    F.Adapter->Policy = EWeaverReferencePolicy::Reject;
    F.Widget->GetTimeline()->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Delete, FModifierKeysState(), 0, false, 0, 0));
    TestTrue(TEXT("Rejected Delete keeps selected authoritative object"), F.Widget->GetTimeline()->GetSelection() == Selected);
    F.Adapter->Policy = EWeaverReferencePolicy::Normal;
    F.Widget->GetTimeline()->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Delete, FModifierKeysState(), 0, false, 0, 0));
    TestEqual(TEXT("Delete input routes through authority"), F.Widget->GetTimeline()->GetDisplayedBlocks().Num(), 1);
    TestFalse(TEXT("Successful Delete reconciles selection"), F.Widget->GetTimeline()->GetSelection().IsValid());

    class FBeginSwitchAdapter : public FWeaverTimelineReferenceAdapter
    {
    public:
        virtual void BeginPreview(const FWeaverEditSession& Session) override
        {
            FWeaverTimelineReferenceAdapter::BeginPreview(Session);
            ReplaceDocument();
        }
    };
    auto Switching = MakeShared<FBeginSwitchAdapter>();
    auto SwitchingWidget = SNew(SWeaverEditableTimeline).Adapter(Switching);
    auto P = F.Block(); P.LaneId = Switching->GetDocument().LaneId; P.ItemId = Switching->GetDocument().Items[0].Id;
    TestFalse(TEXT("Source switch inside Begin cancels before return"), SwitchingWidget->GetController()->BeginEdit(P));
    TestEqual(TEXT("Old source sink cleaned"), Switching->Previews.Num(), 0);
    TestEqual(TEXT("Begin replacement ends exactly once"), Switching->EndPreviewCount, 1);
    const auto BeginCancelledReply = SwitchingWidget->GetTimeline()->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D(1120, 240), FSlateLayoutTransform()),
        FPointerEvent(0, FVector2D(308, 42), FVector2D(308, 42), TSet<FKey>{ EKeys::LeftMouseButton },
            EKeys::LeftMouseButton, 0, FModifierKeysState()));
    TestFalse(TEXT("Cancellation in Started cannot acquire ghost mouse capture"), BeginCancelledReply.GetMouseCaptor().IsValid());
    TestEqual(TEXT("Routed begin replacement also clears preview"), Switching->Previews.Num(), 0);

    auto Adapter = MakeShared<FWeaverTimelineReferenceAdapter>();
    TSharedPtr<SWeaverEditableTimeline> Owner = SNew(SWeaverEditableTimeline).Adapter(Adapter);
    auto RetainedChild = Owner->GetTimeline();
    auto RetainedController = Owner->GetController();
    P.LaneId = Adapter->GetDocument().LaneId; P.ItemId = Adapter->GetDocument().Items[0].Id;
    RetainedController->BeginEdit(P);
    Owner.Reset();
    TestFalse(TEXT("Owner destruction terminates retained controller"), RetainedController->HasActiveEdit());
    TestEqual(TEXT("Owner destruction clears sink"), Adapter->Previews.Num(), 0);
    RetainedChild->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Delete, FModifierKeysState(), 0, false, 0, 0));
    TestEqual(TEXT("Retained child cannot call destroyed owner"), Adapter->CommitCount, 0);
    return true;
}
#endif
