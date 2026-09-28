# Run on a disposable Windows runner: NSIS also writes current-user shortcuts
# and uninstall metadata. This validates the installed package, not a Qt SDK.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Installer,
    [Parameter(Mandatory = $true)]
    [string] $InstallRoot,
    [Parameter(Mandatory = $true)]
    [string] $ReportDirectory,
    [ValidateRange(5, 120)]
    [int] $StartupSeconds = 30
)

$ErrorActionPreference = 'Stop'
if ($env:PROCESSOR_ARCHITECTURE -ne 'ARM64') {
    throw 'The ARM64 installer must be tested by native ARM64 PowerShell.'
}
if (Test-Path -LiteralPath $InstallRoot) {
    throw "Use a fresh installation directory: '$InstallRoot'."
}
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$InstallRoot = [IO.Path]::GetFullPath($InstallRoot)
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
$ReportDirectory = (Resolve-Path -LiteralPath $ReportDirectory).Path
$report = [ordered]@{
    installerSha256 = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash
    architecture = $env:PROCESSOR_ARCHITECTURE
    installed = $false
    runtimeVerified = $false
    startupReady = $false
    splashDismissed = $false
    remainedAlive = $false
    graphicsInitializationFailed = $false
    hardware3D = 'not-validated-by-launch-smoke'
}
$installerProcess = $null
$galleryProcess = $null
$stdoutTask = $null
$stderrTask = $null
try {
    # NSIS requires /D last and consumes the remainder as the installation path.
    $installerProcess = Start-Process -FilePath $installerPath `
        -ArgumentList @('/S', "/D=$InstallRoot") -PassThru
    if (-not $installerProcess.WaitForExit(300000)) {
        throw 'The Windows ARM64 installer did not finish within five minutes.'
    }
    if ($installerProcess.ExitCode -ne 0) {
        throw "Installer exited with code $($installerProcess.ExitCode)."
    }
    $report.installed = $true

    $bin = Join-Path $InstallRoot 'bin'
    & "$PSScriptRoot/../../cmake/VerifyWindowsPeArchitecture.ps1" `
        -Root $bin -ExpectedMachine Arm64 -RequireMsvcRuntime -RequireQt6SpatialRuntime
    $report.runtimeVerified = $true

    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = Join-Path $bin 'fluent_qt_gallery.exe'
    $startInfo.WorkingDirectory = $ReportDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    # Restrict lookup to the package and Windows, even on a developer image.
    foreach ($name in @($startInfo.EnvironmentVariables.Keys)) {
        if ($name -match '^(QT_|QML|SPDLOG_|FLUENT_QT_GALLERY_DISABLE_3D$|FLUENT_QT_SPATIAL_RENDERER$)') {
            $startInfo.EnvironmentVariables.Remove($name)
        }
    }
    $startInfo.EnvironmentVariables['PATH'] = "$env:SystemRoot\System32;$env:SystemRoot"
    $startInfo.EnvironmentVariables['QT_QPA_PLATFORM'] = 'windows'
    $startInfo.EnvironmentVariables['SPDLOG_LEVEL'] = 'debug'
    $galleryProcess = [Diagnostics.Process]::Start($startInfo)
    $stdoutTask = $galleryProcess.StandardOutput.ReadToEndAsync()
    $stderrTask = $galleryProcess.StandardError.ReadToEndAsync()
    if ($galleryProcess.WaitForExit($StartupSeconds * 1000)) {
        throw "Installed Gallery exited during startup with code $($galleryProcess.ExitCode)."
    }
    $report.remainedAlive = $true
} catch {
    $report.error = $_.Exception.Message
} finally {
    foreach ($process in @($galleryProcess, $installerProcess)) {
        if ($process -and -not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
    }
    $stdout = if ($stdoutTask) { $stdoutTask.GetAwaiter().GetResult() } else { '' }
    $stderr = if ($stderrTask) { $stderrTask.GetAwaiter().GetResult() } else { '' }
    $stdout | Set-Content -LiteralPath (Join-Path $ReportDirectory 'stdout.log') -Encoding UTF8
    $stderr | Set-Content -LiteralPath (Join-Path $ReportDirectory 'stderr.log') -Encoding UTF8
    $output = "$stdout`n$stderr"
    $report.startupReady = $output.Contains('GalleryWindow startup ready ')
    $report.splashDismissed = $output.Contains('GalleryWindow startup dismissed ')
    # Startup signals can still fire while a failed GPU child leaves the whole
    # window blank. An unsuccessful preflight alone is safe: no GPU child exists.
    $report.graphicsInitializationFailed = $output -match (
        'QOpenGLWidget: Failed to (?:create context|initialize)|' +
        'QRhi[^\r\n]*: Failed to (?:create|initialize)|' +
        'Failed to create (?:backing store RHI|QRhi)'
    )
    $report | ConvertTo-Json | Set-Content `
        -LiteralPath (Join-Path $ReportDirectory 'verification.json') -Encoding UTF8
    if ($galleryProcess) { $galleryProcess.Dispose() }
    if ($installerProcess) { $installerProcess.Dispose() }
}
if ($report.error) {
    throw $report.error
}
if (-not $report.startupReady -or -not $report.splashDismissed) {
    throw "Installed Gallery did not complete startup; see '$ReportDirectory'."
}
if ($report.graphicsInitializationFailed) {
    throw "Installed Gallery failed to initialize its drawing surface; see '$ReportDirectory'."
}
Write-Host "Installed ARM64 Gallery completed startup and remained alive for ${StartupSeconds}s."
Write-Host 'Hardware 3D rendering still requires a capable GPU and native visual validation.'
