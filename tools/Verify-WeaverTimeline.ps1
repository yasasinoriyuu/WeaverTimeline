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

$Failed = $false

foreach ($File in $Files) {
    $Master = Join-Path $CoreRoot $File
    if (-not (Test-Path -LiteralPath $Master)) {
        Write-Error "Missing master source file: $Master"
        $Failed = $true
        continue
    }

    $MasterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Master).Hash

    foreach ($DestinationRoot in $DestinationRoots) {
        $Copy = Join-Path $DestinationRoot $File
        if (-not (Test-Path -LiteralPath $Copy)) {
            Write-Error "Missing copy: $Copy"
            $Failed = $true
            continue
        }

        $CopyHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Copy).Hash
        if ($CopyHash -ne $MasterHash) {
            Write-Error "Mismatch: $Copy"
            Write-Host "  master: $MasterHash"
            Write-Host "  copy:   $CopyHash"
            $Failed = $true
        } else {
            Write-Host "OK $File -> $DestinationRoot [$MasterHash]"
        }
    }
}

if ($Failed) {
    throw 'WeaverTimeline verification failed.'
}

Write-Host 'All WeaverTimeline Core copies are byte-for-byte identical to the source master.'
