[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $Root,

    [Parameter(Mandatory = $true)]
    [ValidateSet('Arm64', 'X64', 'X86')]
    [string] $ExpectedMachine,

    [switch] $RequireMsvcRuntime
)

$ErrorActionPreference = 'Stop'
$resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
$expectedMachineValue = @{
    Arm64 = 0xAA64
    X64 = 0x8664
    X86 = 0x014C
}[$ExpectedMachine]
$machineNames = @{
    0xAA64 = 'Arm64'
    0x8664 = 'X64'
    0x014C = 'X86'
}

function Get-PeMachine {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) {
            return $null
        }

        $stream.Position = 0x3C
        $peOffset = $reader.ReadUInt32()
        if ($peOffset -gt ($stream.Length - 6)) {
            throw "Invalid PE header offset in '$Path'."
        }

        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "Invalid PE signature in '$Path'."
        }
        return $reader.ReadUInt16()
    } finally {
        $stream.Dispose()
    }
}

$peFiles = Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File |
    Where-Object { $_.Extension -in @('.exe', '.dll') }
if (-not $peFiles) {
    throw "No PE executables or DLLs were found below '$resolvedRoot'."
}

$mismatches = @()
foreach ($file in $peFiles) {
    $machine = Get-PeMachine -Path $file.FullName
    if ($null -eq $machine) {
        throw "Expected a PE file but did not find an MZ header: '$($file.FullName)'."
    }

    $machineName = if ($machineNames.ContainsKey([int] $machine)) {
        $machineNames[[int] $machine]
    } else {
        'Unknown(0x{0:X4})' -f $machine
    }
    $relativePath = [IO.Path]::GetRelativePath($resolvedRoot, $file.FullName)
    Write-Host "$machineName`t$relativePath"
    if ($machine -ne $expectedMachineValue) {
        $mismatches += "$relativePath=$machineName"
    }
}

if ($mismatches.Count -gt 0) {
    throw "Expected every packaged PE below '$resolvedRoot' to be $ExpectedMachine; mismatches: $($mismatches -join ', ')."
}

if ($RequireMsvcRuntime) {
    $requiredRuntimeFiles = @(
        'msvcp140.dll',
        'msvcp140_1.dll',
        'vcruntime140.dll',
        'vcruntime140_1.dll'
    )
    $missingRuntimeFiles = @($requiredRuntimeFiles | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $resolvedRoot $_))
    })
    if ($missingRuntimeFiles.Count -gt 0) {
        throw "Packaged MSVC runtime is incomplete below '$resolvedRoot'; missing: $($missingRuntimeFiles -join ', ')."
    }
}

Write-Host "Verified $($peFiles.Count) packaged PE files as $ExpectedMachine below '$resolvedRoot'."
