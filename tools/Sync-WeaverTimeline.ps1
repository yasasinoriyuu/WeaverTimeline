param(
    [Parameter(Mandatory = $true)]
    [string[]] $DestinationRoots
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$CoreRoot = Join-Path $RepoRoot 'Core'
$Files = @(
    'SWeaverTimeline.h',
    'SWeaverTimeline.cpp',
    'WeaverTimelineTypes.h',
    'WeaverTimelineVersion.h'
)

foreach ($DestinationRoot in $DestinationRoots) {
    New-Item -ItemType Directory -Force -Path $DestinationRoot | Out-Null

    foreach ($File in $Files) {
        $Source = Join-Path $CoreRoot $File
        if (-not (Test-Path -LiteralPath $Source)) {
            throw "Missing master source file: $Source"
        }

        $Destination = Join-Path $DestinationRoot $File
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
    }

    Write-Host "Synced WeaverTimeline Core -> $DestinationRoot"
}

Write-Host 'Sync complete. Run Verify-WeaverTimeline.ps1 to confirm byte-for-byte equality.'
