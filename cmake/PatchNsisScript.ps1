[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $NsiPath
)

$ErrorActionPreference = 'Stop'
$resolvedNsiPath = (Resolve-Path -LiteralPath $NsiPath).Path
$content = [IO.File]::ReadAllText($resolvedNsiPath)
$marker = "; Fluent-Qt modern per-user installer"

if ($content.Contains($marker)) {
    return
}

$newline = if ($content.Contains("`r`n")) { "`r`n" } else { "`n" }

function Replace-Required {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Source,
        [Parameter(Mandatory = $true)]
        [string] $Target,
        [Parameter(Mandatory = $true)]
        [string] $Replacement,
        [Parameter(Mandatory = $true)]
        [string] $Description
    )

    if (-not $Source.Contains($Target)) {
        throw "Cannot patch generated NSIS script: missing $Description anchor."
    }
    return $Source.Replace($Target, $Replacement)
}

# CPack's NSIS template defaults to a machine-wide install. Gallery is a
# current-user application, so keep elevation, shell folders, and uninstall
# metadata in the current user's scope.
$content = Replace-Required $content `
    "RequestExecutionLevel admin" `
    "RequestExecutionLevel user" `
    "execution level"
$content = $content -replace `
    'InstallDir "\$PROGRAMFILES(?:64|32)?\\([^\"]+)"', `
    'InstallDir "$$LOCALAPPDATA\Programs\$1"'
$content = $content -replace `
    'StrCpy \$INSTDIR "\$DOCUMENTS\\([^\"]+)"', `
    'StrCpy $$INSTDIR "$$LOCALAPPDATA\Programs\$1"'
$content = $content -replace `
    '\$PROGRAMFILES(?:64|32)?\\', `
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

$nameMatch = [regex]::Match($content, '(?m)^\s*Name "([^\"]+)"')
if (-not $nameMatch.Success) {
    throw "Cannot patch generated NSIS script: package display name was not found."
}
$displayName = $nameMatch.Groups[1].Value

# Inject project-owned Modern UI settings without copying CMake's full NSIS
# template. The include contains only presentation copy and theme defaults.
$modernUiInclude = (Resolve-Path -LiteralPath (
    Join-Path $PSScriptRoot 'FluentQTNsisModernUi.nsh')).Path
$pageMarker = ";--------------------------------${newline};Pages"
$modernUiBlock = @(
    $marker,
    ('!include "{0}"' -f $modernUiInclude),
    '',
    $pageMarker
) -join $newline
$content = Replace-Required $content `
    $pageMarker `
    $modernUiBlock `
    "page declaration"

# CPACK_NSIS_MODIFY_PATH is disabled and desktop/Start shortcuts are created
# automatically, so CPack's empty Install Options page adds no value.
$content = $content.Replace(
    "  Page custom InstallOptionsPage${newline}",
    '')

# Preserve CPack's Start Menu bookkeeping macros while skipping their visible
# page. This keeps upgrade/uninstall behavior intact and shortens setup.
$startMenuPage = '  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER'
$startMenuPageReplacement = @(
    '  !define MUI_PAGE_CUSTOMFUNCTION_PRE FluentQtSkipStartMenuPage',
    $startMenuPage
) -join $newline
$content = Replace-Required $content `
    $startMenuPage `
    $startMenuPageReplacement `
    "Start Menu page"

$escapedDisplayName = $displayName.Replace('$', '$$').Replace('"', '$\"')
$skipFunction = @(
    'Function FluentQtSkipStartMenuPage',
    ('  StrCpy $STARTMENU_FOLDER "{0}"' -f $escapedDisplayName),
    '  Abort',
    'FunctionEnd',
    '',
    'Function InstallOptionsPage'
) -join $newline
$content = Replace-Required $content `
    'Function InstallOptionsPage' `
    $skipFunction `
    "Install Options function"

# Give each standard MUI page concise Fluent-oriented headings. Page-specific
# defines are consumed by the next MUI macro and do not leak to later pages.
$pageHeaders = @(
    @(
        '  !insertmacro MUI_PAGE_LICENSE',
        '  !define MUI_PAGE_HEADER_TEXT "Review the license"',
        '  !define MUI_PAGE_HEADER_SUBTEXT "MIT License terms for Fluent-Qt Gallery."'
    ),
    @(
        '  !insertmacro MUI_PAGE_DIRECTORY',
        '  !define MUI_PAGE_HEADER_TEXT "Choose an install location"',
        '  !define MUI_PAGE_HEADER_SUBTEXT "Installed only for the current Windows user."'
    ),
    @(
        '  !insertmacro MUI_PAGE_INSTFILES',
        '  !define MUI_PAGE_HEADER_TEXT "Installing Fluent-Qt Gallery"',
        '  !define MUI_PAGE_HEADER_SUBTEXT "Copying the application and creating shortcuts."'
    ),
    @(
        '  !insertmacro MUI_UNPAGE_CONFIRM',
        '  !define MUI_PAGE_HEADER_TEXT "Uninstall Fluent-Qt Gallery"',
        '  !define MUI_PAGE_HEADER_SUBTEXT "Remove the application and its shortcuts."'
    )
)

foreach ($pageHeader in $pageHeaders) {
    $macro = $pageHeader[0]
    $replacement = @($pageHeader[1], $pageHeader[2], $macro) -join $newline
    $content = Replace-Required $content $macro $replacement $macro
}

$utf8NoBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($resolvedNsiPath, $content, $utf8NoBom)
