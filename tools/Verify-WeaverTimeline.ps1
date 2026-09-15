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

$Failed = $false

foreach ($MasterFile in $Files) {
    $MasterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $MasterFile.FullName).Hash

    foreach ($DestinationRoot in $DestinationRoots) {
        $Copy = Join-Path $DestinationRoot $MasterFile.Name
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
            Write-Host "OK $($MasterFile.Name) -> $DestinationRoot [$MasterHash]"
        }
    }
}

if ($Failed) {
    throw 'WeaverTimeline verification failed.'
}

Write-Host "All $($Files.Count) WeaverTimeline Core files are byte-for-byte identical to the source master in every destination."
