# Run only on a disposable Windows CI runner.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $BuildDirectory,
    [Parameter(Mandatory = $true)]
    [string] $ReportDirectory,
    [switch] $PageHeap,
    [switch] $Bindings
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
$suiteKind = if ($Bindings) { 'bindings' } else { 'foundation' }
if ($Bindings) {
    $test = (Resolve-Path "$PSScriptRoot/../../bindings/pyside6/tests/test_bindings.py").Path
}
$excludedTests = Join-Path $build 'bindings/pyside6/isolated-binding-tests.txt'
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
if ($Bindings) {
    $mode = "bindings-$mode"
}
$stackLog = Join-Path $report "native-stack-$mode.log"
$resultPath = Join-Path $report "probe-result-$mode.json"
$probe = Join-Path $report "repeat-$suiteKind.py"
@'
import json
import os
from pathlib import Path
import re
import runpy
import sys
import unittest
if sys.argv[3] == 'bindings':
    test_path = Path(sys.argv[1]).resolve()
    cmake = (test_path.parents[3] / 'CMakeLists.txt').read_text(encoding='utf-8')
    version = re.search(r'project\s*\(\s*FluentQt\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)', cmake)
    if version is None:
        raise RuntimeError('Cannot resolve the expected binding version')
    os.environ['FLUENTQT_EXPECTED_VERSION'] = version.group(1)
    sys.path.insert(0, str(test_path.parent))
    import test_bindings
    from run_binding_tests import binding_suite
    excluded = set(Path(sys.argv[4]).read_text(encoding='utf-8').splitlines())
    suite = binding_suite(test_bindings, excluded)
    suite.addTest(test_bindings.FluentQtBindingTest(
        'test_command_palette_callbacks_can_destroy_the_palette'))
else:
    case = runpy.run_path(sys.argv[1], run_name='foundation_probe')['FoundationAuthoringTest']
    suite = unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromTestCase(case) for _ in range(25))
expected_tests = suite.countTestCases()
result = unittest.TextTestRunner(verbosity=2, failfast=True).run(suite)
Path(sys.argv[2]).write_text(json.dumps({
    'success': result.wasSuccessful(), 'tests_run': result.testsRun,
    'expected_tests': expected_tests,
    'failures': len(result.failures), 'errors': len(result.errors),
}) + '\n', encoding='utf-8')
raise SystemExit(not result.wasSuccessful())
'@ | Set-Content -LiteralPath $probe -Encoding utf8
foreach ($name in @('fluent_binding_stategroup_wrapper.cpp', 'fluent_binding_stategroup_wrapper.h')) {
    Copy-Item -LiteralPath (Join-Path $build "bindings/pyside6/generated/_fluentqt/$name") `
        -Destination $report
}
if ($Bindings) {
    foreach ($stem in @(
        'fluent_date_time_calendardatepicker_wrapper',
        'fluent_textfields_codeblock_wrapper',
        'fluent_menus_toolbars_commandpalette_wrapper'
    )) {
        foreach ($extension in @('cpp', 'h')) {
            Copy-Item -LiteralPath (Join-Path $build "bindings/pyside6/generated/_fluentqt/$stem.$extension") `
                -Destination $report
        }
    }
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
        $python, '-X', 'faulthandler', $probe, $test, $resultPath, $suiteKind, $excludedTests
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
        throw 'Native lifetime diagnostics captured a fatal exception.'
    }
    $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    if (!$result.success -or $result.expected_tests -le 0 -or
        $result.tests_run -ne $result.expected_tests) {
        throw "Repeated lifetime assertions failed: $(ConvertTo-Json $result -Compress)"
    }
    Write-Host "Verified $($result.tests_run) lifetime assertions under $mode."
} finally {
    if ($process -and !$process.HasExited) {
        $process.Kill($true)
    }
    & $gflags /p /disable python.exe
}
