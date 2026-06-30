[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $Arguments
)

$makensisCommand = Get-Command makensis.exe -ErrorAction SilentlyContinue
if (-not $makensisCommand) {
    $makensisCommand = Get-Command makensis -ErrorAction SilentlyContinue
}
if (-not $makensisCommand) {
    throw "makensis was not found on PATH."
}

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
    $content = [IO.File]::ReadAllText($nsiPath)

    $content = $content.Replace(
        "RequestExecutionLevel admin",
        "RequestExecutionLevel user")
    $content = $content -replace
        'InstallDir "\$PROGRAMFILES(?:64|32)?\\([^"]+)"',
        'InstallDir "$$LOCALAPPDATA\Programs\$1"'
    $content = $content -replace
        'StrCpy \$INSTDIR "\$DOCUMENTS\\([^"]+)"',
        'StrCpy $$INSTDIR "$$LOCALAPPDATA\Programs\$1"'
    $content = $content -replace
        '\$PROGRAMFILES(?:64|32)?\\',
        '$$LOCALAPPDATA\Programs\'
    $content = $content.Replace(
        'SetShellVarContext all',
        'SetShellVarContext current')
    $content = $content.Replace(
        'StrCpy $SV_ALLUSERS "AllUsers"',
        'StrCpy $SV_ALLUSERS "JustMe"')
    $content = $content.Replace(
        'HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\',
        'HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\')

    $utf8NoBom = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllText($nsiPath, $content, $utf8NoBom)
}

& $makensisCommand.Source @Arguments
exit $LASTEXITCODE
