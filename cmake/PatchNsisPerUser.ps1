[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $Arguments
)

$nsiPath = $null
foreach ($argument in $Arguments) {
    $candidate = $argument.Trim('"')
    if ($candidate.EndsWith(".nsi", [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $candidate)) {
        $nsiPath = (Resolve-Path -LiteralPath $candidate).Path
        break
    }
}

if ($nsiPath) {
    & "$PSScriptRoot\PatchNsisScript.ps1" -NsiPath $nsiPath
}

$makensisCommand = Get-Command makensis.exe -ErrorAction SilentlyContinue
if (-not $makensisCommand) {
    $makensisCommand = Get-Command makensis -ErrorAction SilentlyContinue
}
$makensisPath = if ($makensisCommand) { $makensisCommand.Source } else { $null }

# Local development may use an extracted, ignored NSIS ZIP instead of a
# machine-wide installation. Prefer its top-level launcher so NSIS resolves
# its Include, Plugins, and Contrib directories relative to the ZIP root.
if (-not $makensisPath) {
    $portableRoot = Join-Path $PSScriptRoot '..\build\nsis-toolchain'
    if (Test-Path -LiteralPath $portableRoot) {
        $portableCandidates = Get-ChildItem -LiteralPath $portableRoot `
            -Directory -Filter 'nsis-*' -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object { Join-Path $_.FullName 'makensis.exe' } |
            Where-Object { Test-Path -LiteralPath $_ }
        $makensisPath = $portableCandidates | Select-Object -First 1
    }
}

if (-not $makensisPath) {
    throw "makensis was not found on PATH."
}

& $makensisPath @Arguments
exit $LASTEXITCODE
