param(
    [Parameter(Mandatory = $true)]
    [string[]] $DestinationRoots
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$CoreRoot = Join-Path $RepoRoot 'Core'
$Files = Get-ChildItem -LiteralPath $CoreRoot -File | Sort-Object Name

if ($Files.Count -eq 0) {
    throw "No source-master files found in $CoreRoot"
}

foreach ($DestinationRoot in $DestinationRoots) {
    New-Item -ItemType Directory -Force -Path $DestinationRoot | Out-Null

    foreach ($SourceFile in $Files) {
        $Destination = Join-Path $DestinationRoot $SourceFile.Name
        Copy-Item -LiteralPath $SourceFile.FullName -Destination $Destination -Force
    }

    Write-Host "Synced $($Files.Count) WeaverTimeline Core files -> $DestinationRoot"
}

Write-Host 'Sync complete. Run Verify-WeaverTimeline.ps1 to confirm byte-for-byte equality.'
