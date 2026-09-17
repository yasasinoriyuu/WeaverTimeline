param(
    [Parameter(Mandatory = $true)][string] $OutputRoot
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$ResolvedOutput = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\', '/')
if ($ResolvedOutput -match '[^\x00-\x7F]' -or $ResolvedOutput.StartsWith($RepoRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use an ASCII output path outside the source-master repository.'
}
$Marker = Join-Path $ResolvedOutput '.weaver-reference-generated'
if ((Test-Path -LiteralPath $ResolvedOutput) -and -not (Test-Path -LiteralPath $Marker)) {
    throw 'Refusing to overwrite an existing directory without the reference-project marker.'
}
New-Item -ItemType Directory -Force -Path $ResolvedOutput | Out-Null
Set-Content -LiteralPath $Marker -Value 'Generated WeaverTimeline reference validation host' -Encoding utf8
$PluginRoot = Join-Path $ResolvedOutput 'Plugins\WeaverTimelineReference'
New-Item -ItemType Directory -Force -Path $PluginRoot | Out-Null
Copy-Item -LiteralPath (Join-Path $RepoRoot 'Reference\WeaverTimelineReference.uplugin') -Destination $PluginRoot -Force
Copy-Item -LiteralPath (Join-Path $RepoRoot 'Reference\Source') -Destination $PluginRoot -Recurse -Force
$CoreDestination = Join-Path $PluginRoot 'Source\WeaverTimelineReference\Private\WeaverTimeline'
& (Join-Path $PSScriptRoot 'Sync-WeaverTimeline.ps1') -DestinationRoots $CoreDestination
& (Join-Path $PSScriptRoot 'Verify-WeaverTimeline.ps1') -DestinationRoots $CoreDestination
$ModuleRoot = Join-Path $ResolvedOutput 'Source\WeaverReference'
$ConfigRoot = Join-Path $ResolvedOutput 'Config'
New-Item -ItemType Directory -Force -Path $ModuleRoot, $ConfigRoot | Out-Null
@'
{
    "FileVersion": 3,
    "EngineAssociation": "5.8",
    "Modules": [{ "Name": "WeaverReference", "Type": "Runtime", "LoadingPhase": "Default" }],
    "Plugins": [{ "Name": "WeaverTimelineReference", "Enabled": true }]
}
'@ | Set-Content -LiteralPath (Join-Path $ResolvedOutput 'WeaverReference.uproject') -Encoding utf8
@'
using UnrealBuildTool;
public class WeaverReferenceEditorTarget : TargetRules
{
    public WeaverReferenceEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("WeaverReference");
    }
}
'@ | Set-Content -LiteralPath (Join-Path $ResolvedOutput 'Source\WeaverReferenceEditor.Target.cs') -Encoding utf8
@'
using UnrealBuildTool;
public class WeaverReference : ModuleRules
{
    public WeaverReference(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
'@ | Set-Content -LiteralPath (Join-Path $ModuleRoot 'WeaverReference.Build.cs') -Encoding utf8
@'
#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, WeaverReference, "WeaverReference");
'@ | Set-Content -LiteralPath (Join-Path $ModuleRoot 'WeaverReference.cpp') -Encoding utf8
@'
[Zen]
AutoLaunch=false
[Zen.ConnectExisting]
HostName=127.0.0.1
Port=8558
'@ | Set-Content -LiteralPath (Join-Path $ConfigRoot 'DefaultEngine.ini') -Encoding utf8
Write-Host "Reference host ready: $ResolvedOutput\WeaverReference.uproject"
