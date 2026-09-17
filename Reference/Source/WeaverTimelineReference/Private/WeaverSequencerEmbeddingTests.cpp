#include "Misc/AutomationTest.h"
#include "WeaverReferenceTrack.h"
#include "WeaverSequencerSection.h"
#include "SWeaverEditableTimeline.h"
#include "WeaverTimelineReferenceAdapter.h"
#include "ISequencerModule.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "LevelEditorSequencerIntegration.h"
#include "ViewRangeInterpolation.h"
#include "AnimatedRange.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SWindow.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
TSharedPtr<SWeaverEditableTimeline> FindEmbedded(const TSharedRef<SWidget>& Root)
{
    if (Root->GetTypeAsString() == TEXT("SWeaverEditableTimeline")) { return StaticCastSharedRef<SWeaverEditableTimeline>(Root); }
    for (int32 I = 0; I < Root->GetChildren()->Num(); ++I)
    {
        if (auto Found = FindEmbedded(Root->GetChildren()->GetChildAt(I))) { return Found; }
    }
    return nullptr;
}
struct FNativeFixture
{
    TStrongObjectPtr<ULevelSequence> Sequence;
    TSharedPtr<ISequencer> Sequencer;
    TSharedPtr<SWindow> Window;
    UWeaverReferenceSection* Section = nullptr;
    FNativeFixture()
    {
        Sequence.Reset(NewObject<ULevelSequence>(GetTransientPackage(), NAME_None, RF_Transactional));
        Sequence->Initialize();
        auto* Scene = Sequence->GetMovieScene();
        Scene->SetDisplayRate(FFrameRate(30, 1));
        auto* Track = Scene->AddTrack<UWeaverReferenceTrack>();
        Section = CastChecked<UWeaverReferenceSection>(Track->CreateNewSection());
        Track->AddSection(*Section);
        FSequencerInitParams Params;
        Params.RootSequence = Sequence.Get();
        Params.ViewParams.UniqueName = TEXT("WeaverV7EmbeddedTest");
        Sequencer = FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer")).CreateSequencer(Params);
        Sequencer->SetViewRange(TRange<double>(0, 8), EViewRangeInterpolation::Immediate);
        Window = SNew(SWindow).ClientSize(FVector2D(1200, 600))[Sequencer->GetSequencerWidget()];
        // Native virtualized track rows are constructed only for a shown window.
        FSlateApplication::Get().AddWindow(Window.ToSharedRef(), true);
        Window->SlatePrepass(); // Let the normal editor loop build virtualized rows; do not nest Slate ticks.
    }
    ~FNativeFixture()
    {
        if (Sequencer) { Sequencer->Close(); }
        FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverNativeEmbeddingTest, "WeaverTimeline.Embedded.NativeRootInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWeaverNativeEmbeddingTest::RunTest(const FString&)
{
    auto FixturePtr = MakeShared<FNativeFixture>();
    auto Stage = MakeShared<int32>(0);
    const double Deadline = FPlatformTime::Seconds() + 5.0;
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, FixturePtr, Stage, Deadline]()
    {
    auto& Fixture = *FixturePtr;
    auto Widget = FindEmbedded(Fixture.Sequencer->GetSequencerWidget());
    if (!Widget && FPlatformTime::Seconds() < Deadline) { return false; }
    if (!TestTrue(TEXT("Real native root section created an embedded editable widget within 5 seconds"), Widget.IsValid())) { return true; }
    // Each latent step discovers the current native tree. Undo can replace section interfaces.
    if (*Stage == 1)
    {
        TestEqual(TEXT("Native embedded Undo restores authority"), Fixture.Section->Document->Items[0].Start, 20.0);
        TestEqual(TEXT("Native embedded Undo restores CURRENT display"), Widget->GetTimeline()->GetDisplayedBlocks()[0].StartFrame, 20.0);
        GEditor->RedoTransaction();
        *Stage = 2;
        return false;
    }
    if (*Stage == 3)
    {
        // No FWidgetPath retaining the SSequencer hierarchy survives across this close.
        Fixture.Sequencer->Close();
        TestFalse(TEXT("Closing owner cancels capture synchronously"), Widget->GetTimeline()->HasMouseCapture());
        TestFalse(TEXT("Closing owner cancels controller session"), Widget->GetController()->HasActiveEdit());
        Widget->GetController()->FinishEdit(80, 120, false);
        TestTrue(TEXT("Late finish after close cannot commit"), FMath::IsNearlyEqual(Fixture.Section->Document->Items[0].Start, 50.0, .2));
        Fixture.Sequencer.Reset();
        return true;
    }
    TestTrue(TEXT("Anchor has unlimited layout range"), Fixture.Section->GetRange() == TRange<FFrameNumber>::All());
    TestTrue(TEXT("Native anchor locked"), Fixture.Section->IsLocked());
    TestEqual(TEXT("Only one root track, no binding child substitute"), Fixture.Sequence->GetMovieScene()->GetTracks().Num(), 1);
    FWidgetPath Layout;
    FSlateApplication::Get().GeneratePathToWidgetChecked(Widget->GetTimeline(), Layout, EVisibility::All);
    const FGeometry Geometry = Layout.Widgets.Last().Geometry;
    // Existence precedes arrangement for virtualized native rows. Wait for real layout.
    if (Geometry.GetLocalSize().Y < 36 && FPlatformTime::Seconds() < Deadline) { return false; }
    TestTrue(TEXT("Mounted native section has usable width"), Geometry.GetLocalSize().X > 400);
    TestTrue(TEXT("Mounted native section has lane height"), Geometry.GetLocalSize().Y >= 36);
    AddInfo(FString::Printf(TEXT("Native content geometry %.2f x %.2f"), Geometry.GetLocalSize().X, Geometry.GetLocalSize().Y));
    TArray<FWidgetAndPointer> Nodes;
    for (int32 I = 0; I < Layout.Widgets.Num(); ++I) { Nodes.Add(FWidgetAndPointer(Layout.Widgets[I])); }
    FWidgetPath Path(MakeArrayView(Nodes));
    const auto At = [&](double Frame) { return Geometry.LocalToAbsolute(FVector2D(Frame / 240.0 * Geometry.GetLocalSize().X, 18)); };
    const auto Pointer = [&](double Frame, double Previous, bool Down)
    {
        TSet<FKey> Buttons;
        if (Down) { Buttons.Add(EKeys::LeftMouseButton); }
        return FPointerEvent(0, At(Frame), At(Previous), Buttons, EKeys::LeftMouseButton, 0, FModifierKeysState());
    };
    auto& Slate = FSlateApplication::Get();
    if (*Stage == 2)
    {
        TestTrue(TEXT("Native embedded Redo restores authority"), FMath::IsNearlyEqual(Fixture.Section->Document->Items[0].Start, 50.0, .2));
        TestTrue(TEXT("Native embedded Redo restores CURRENT display"), FMath::IsNearlyEqual(Widget->GetTimeline()->GetDisplayedBlocks()[0].StartFrame, 50.0, .2));
        Slate.RoutePointerDownEvent(Path, Pointer(70, 70, true));
        Slate.RoutePointerMoveEvent(Path, Pointer(100, 70, true), false);
        TestTrue(TEXT("Real routed drag owns capture before source close"), Widget->GetTimeline()->HasMouseCapture());
        TestTrue(TEXT("Real routed drag has active session before source close"), Widget->GetController()->HasActiveEdit());
        *Stage = 3;
        return false;
    }
    Slate.RoutePointerDownEvent(Path, Pointer(40, 40, true));
    Slate.RoutePointerMoveEvent(Path, Pointer(70, 40, true), false);
    Slate.RoutePointerUpEvent(Path, Pointer(70, 70, false));
    TestTrue(TEXT("Native embedded drag writes document 20->50 (subpixel tolerance 0.2 frame)"),
        FMath::IsNearlyEqual(Fixture.Section->Document->Items[0].Start, 50.0, .2));
    TestTrue(TEXT("Native embedded UI rereads actual document"), FMath::IsNearlyEqual(
        Widget->GetTimeline()->GetDisplayedBlocks()[0].StartFrame, Fixture.Section->Document->Items[0].Start, .001));
    GEditor->UndoTransaction();
    *Stage = 1;
    return false;
    }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaverOwnerIsolationTest, "WeaverTimeline.Embedded.ExplicitOwnerIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWeaverOwnerIsolationTest::RunTest(const FString&)
{
    auto APtr = MakeShared<FNativeFixture>();
    auto BPtr = MakeShared<FNativeFixture>();
    const double Deadline = FPlatformTime::Seconds() + 5.0;
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, APtr, BPtr, Deadline]()
    {
    auto& A = *APtr;
    auto& B = *BPtr;
    auto Widget = FindEmbedded(B.Sequencer->GetSequencerWidget());
    if (!Widget && FPlatformTime::Seconds() < Deadline) { return false; }
    if (!TestTrue(TEXT("Second owner widget exists within 5 seconds"), Widget.IsValid())) { return true; }
    FWeaverSequencerBridge Bridge;
    int32 Changes = 0;
    double Received = -1;
    Bridge.RegisterForSequencer(Widget->GetTimeline(), B.Sequencer, {},
        FOnWeaverSequencerFrameChanged::CreateLambda([&](double Frame) { Received = Frame; }),
        FSimpleDelegate::CreateLambda([&]() { ++Changes; }));
    Bridge.PushTimelineFrame(90);
    TestEqual(TEXT("Explicit owner B scrubbed to 3 seconds"), B.Sequencer->GetLocalTime().AsSeconds(), 3.0);
    TestEqual(TEXT("Other owner A untouched"), A.Sequencer->GetLocalTime().AsSeconds(), 0.0);
    TestEqual(TEXT("Explicit reverse sync receives frame 90"), Received, 90.0);
    Bridge.PushTimelineViewRange(30, 150);
    TestEqual(TEXT("Explicit owner range starts at 1 second"), B.Sequencer->GetViewRange().GetLowerBoundValue(), 1.0);
    B.Sequencer->Close();
    TestFalse(TEXT("Close detaches even while strong ISequencer reference remains alive"), Bridge.IsBound());
    const auto TimeAtClose = B.Sequencer->GetLocalTime().Time;
    Bridge.PushTimelineFrame(180);
    Bridge.Sync(FGeometry::MakeRoot(FVector2D(800, 100), FSlateLayoutTransform()));
    TestFalse(TEXT("Closed explicit owner cannot be rediscovered"), Bridge.IsBound());
    TestTrue(TEXT("No write to closed owner"), B.Sequencer->GetLocalTime().Time == TimeAtClose);
    TestEqual(TEXT("No fallback write to other owner"), A.Sequencer->GetLocalTime().AsSeconds(), 0.0);
    TestEqual(TEXT("One attach and one close context notification"), Changes, 2);
    Bridge.Unregister();
    B.Sequencer.Reset();
    return true;
    }));
    return true;
}
#endif
