# Run only on a disposable Windows CI runner.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $BuildDirectory,
    [Parameter(Mandatory = $true)]
    [string] $ReportDirectory,
    [switch] $PageHeap
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
$report = (Resolve-Path -LiteralPath $ReportDirectory).Path
$debuggers = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/Debuggers/x64'
$cdb = Join-Path $debuggers 'cdb.exe'
$gflags = Join-Path $debuggers 'gflags.exe'

if (!(Test-Path -LiteralPath $cdb)) {
    # Microsoft's 10.0.26100.9457 installer; install only the debugger feature.
    $installer = Join-Path $report 'winsdksetup.exe'
    Invoke-WebRequest -Uri 'https://go.microsoft.com/fwlink/?linkid=2382321' `
        -OutFile $installer
    $signature = Get-AuthenticodeSignature -LiteralPath $installer
    if ($signature.Status -ne 'Valid' -or
        $signature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') {
        throw 'The Windows SDK installer does not have a valid Microsoft signature.'
    }
    $setup = Start-Process -FilePath $installer -ArgumentList @(
        '/quiet', '/norestart', '/features', 'OptionId.WindowsDesktopDebuggers'
    ) -PassThru
    if (!$setup.WaitForExit(300000)) {
        $setup.Kill($true)
        throw 'Debugger installation exceeded five minutes.'
    }
    if ($setup.ExitCode -notin @(0, 3010)) {
        throw "Debugger installation exited with $($setup.ExitCode)."
    }
}
if (!(Test-Path -LiteralPath $cdb) -or !(Test-Path -LiteralPath $gflags)) {
    throw 'The Windows SDK did not provide CDB and GFlags.'
}

$python = (Get-Command python).Source
$test = (Resolve-Path "$PSScriptRoot/../../bindings/pyside6/tests/test_foundation_authoring.py").Path
$env:PYTHONPATH = Join-Path $build 'python'
$env:QT_QPA_PLATFORM = 'offscreen'
$env:_NO_DEBUG_HEAP = '1'
Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue
Remove-Item Env:QT_QPA_PLATFORM_PLUGIN_PATH -ErrorAction SilentlyContinue
$symbols = "$(Join-Path $build 'python/fluentqt');srv*$(Join-Path $report 'symbols')*https://msdl.microsoft.com/download/symbols"
$onFault = '.echo FLUENTQT_NATIVE_LIFETIME_FAULT; .exr -1; .ecxr; kb 100; !analyze -v; q'
$filters = @('av', '0xc0000409', '0xc0000374', 'bpe') | ForEach-Object {
    "sxe -c `"$onFault`" -c2 `"$onFault`" $_"
}
$commands = ($filters -join '; ') + '; g'
$mode = if ($PageHeap) { 'page-heap' } else { 'ordinary-heap' }
$stackLog = Join-Path $report "native-stack-$mode.log"
$probe = Join-Path $report 'repeat-foundation.py'
@'
import runpy
import sys
import unittest
case = runpy.run_path(sys.argv[1], run_name='foundation_probe')['FoundationAuthoringTest']
suite = unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromTestCase(case) for _ in range(25))
result = unittest.TextTestRunner(verbosity=2, failfast=True).run(suite)
raise SystemExit(not result.wasSuccessful())
'@ | Set-Content -LiteralPath $probe -Encoding utf8
foreach ($name in @('fluent_binding_stategroup_wrapper.cpp', 'fluent_binding_stategroup_wrapper.h')) {
    Copy-Item -LiteralPath (Join-Path $build "bindings/pyside6/generated/_fluentqt/$name") `
        -Destination $report
}
$process = $null
try {
    if ($PageHeap) {
        & $gflags /p /enable python.exe /full
    } else {
        & $gflags /p /disable python.exe
    }
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $cdb
    $start.UseShellExecute = $false
    foreach ($argument in @(
        '-G', '-logo', $stackLog,
        '-y', $symbols, '-c', $commands,
        $python, '-X', 'faulthandler', $probe, $test
    )) {
        $start.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::Start($start)
    if (!$process.WaitForExit(300000)) {
        $process.Kill($true)
        throw 'Native lifetime diagnostics exceeded five minutes.'
    }
    Write-Host "CDB diagnostic exit code: $($process.ExitCode)"
    if ($process.ExitCode -ne 0) {
        throw "Native lifetime diagnostics exited with $($process.ExitCode)."
    }
    if (Select-String -LiteralPath $stackLog -Pattern '^FLUENTQT_NATIVE_LIFETIME_FAULT' -Quiet) {
        throw 'Native lifetime diagnostics captured an invalid memory access.'
    }
} finally {
    if ($process -and !$process.HasExited) {
        $process.Kill($true)
    }
    & $gflags /p /disable python.exe
}
