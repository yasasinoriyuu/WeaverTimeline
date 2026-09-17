using UnrealBuildTool;

public class WeaverTimelineReference : ModuleRules
{
    public WeaverTimelineReference(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "Slate", "SlateCore", "InputCore",
            "UnrealEd", "LevelEditor", "MovieScene", "Sequencer", "SequencerWidgets", "LevelSequence"
        });
        PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Private", "WeaverTimeline"));
    }
}
