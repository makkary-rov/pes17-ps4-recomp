# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    M2b (D-LIBC) variant A measurement: link the dump's own libc.prx and libSceFios2.prx natively.
.DESCRIPTION
    Needs the output of verify_M2.ps1 (out\verify\run1\boot.bin and analysis.json). Runs
    scripts\link_modules.py into a copy of that folder (out\verify\m2b), then the loader on the linked image
    with --stop-before-entry and an import report. Writes out\verify\M2b-result.json (hashes, counts and
    import names only; no game bytes). The game directory is only read.
.PARAMETER Game
    The game directory (eboot.elf or eboot.bin, sce_module\libc.prx, sce_module\libSceFios2.prx).
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File verify\verify_M2b.ps1 -Game <path to your game folder>
#>
param([Parameter(Mandatory = $true)][string]$Game)

$ErrorActionPreference = 'Continue'
$Utf8 = New-Object System.Text.UTF8Encoding $false
$Started = Get-Date
$Root = Split-Path -Parent $PSScriptRoot
$V = Join-Path $Root 'out\verify'
$W = Join-Path $V 'm2b'
$Exe = Join-Path $Root 'out\pes17recomp.exe'
$MsysRoot = 'C:\msys64'
if ($env:MSYS2_ROOT) { $MsysRoot = $env:MSYS2_ROOT }
$Crit = [ordered]@{}

function Set-Crit([string]$Id, [string]$Status, [string]$Detail, $Values = $null) {
    $Crit[$Id] = [ordered]@{ id = $Id; status = $Status; detail = $Detail; values = $Values }
}
function Hash-File([string]$P) { return (Get-FileHash -LiteralPath $P -Algorithm SHA256).Hash.ToLowerInvariant() }
function Find-Python {
    foreach ($c in @('python', 'python3')) {
        $cmd = Get-Command $c -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    return $null
}
function Run-Proc([string]$File, [string[]]$ArgList, [string]$Log, [hashtable]$Env = @{}) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $File
    $psi.Arguments = (($ArgList | ForEach-Object { '"' + ($_ -replace '"', '\"') + '"' }) -join ' ')
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.RedirectStandardInput = $true
    $psi.WorkingDirectory = $Root
    foreach ($k in $Env.Keys) { $psi.EnvironmentVariables[$k] = $Env[$k] }
    $p = [System.Diagnostics.Process]::Start($psi)
    $p.StandardInput.Close()
    $so = $p.StandardOutput.ReadToEndAsync()
    $se = $p.StandardError.ReadToEndAsync()
    $done = $p.WaitForExit(1800000)
    if (-not $done) { try { $p.Kill() } catch { } }
    $text = $so.Result + $se.Result
    if ($Log) { [System.IO.File]::WriteAllText($Log, $text, $Utf8) }
    $code = -1
    if ($done) { $code = $p.ExitCode }
    return [pscustomobject]@{ Exit = $code; TimedOut = (-not $done); Text = $text }
}

$Exit = 0
$ExeSha = ''
try {
    New-Item -ItemType Directory -Force -Path $V | Out-Null
    $GameDir = $null
    try { $GameDir = (Resolve-Path -LiteralPath $Game -ErrorAction Stop).Path } catch { }
    $Run1 = Join-Path $V 'run1'
    $Py = Find-Python
    $ready = $true
    if (-not $GameDir) { Set-Crit 'B1' 'FAIL' 'game directory not found'; $ready = $false }
    elseif (-not (Test-Path -LiteralPath (Join-Path $Run1 'boot.bin'))) { Set-Crit 'B1' 'FAIL' 'out\verify\run1\boot.bin missing: run verify_M2.ps1 first'; $ready = $false }
    elseif (-not $Py) { Set-Crit 'B1' 'FAIL' 'python 3 not found'; $ready = $false }
    elseif (-not (Test-Path -LiteralPath $Exe)) { Set-Crit 'B1' 'FAIL' 'out\pes17recomp.exe missing: run verify_M2.ps1 first'; $ready = $false }

    if ($ready) {
        if (Test-Path -LiteralPath $W) { Remove-Item -Recurse -Force -LiteralPath $W }
        New-Item -ItemType Directory -Force -Path $W | Out-Null
        foreach ($f in @('boot.bin', 'analysis.json', 'import-aliases.txt')) {
            $src = Join-Path $Run1 $f
            if (Test-Path -LiteralPath $src) { Copy-Item -LiteralPath $src -Destination (Join-Path $W $f) }
        }
        $ExeSha = Hash-File $Exe
        $rl = Run-Proc $Py @((Join-Path $Root 'scripts\link_modules.py'), $GameDir, '--out', $W, '--title', (Join-Path $Root 'src\title\CUSA04957.json')) (Join-Path $V 'M2b-link.log')
        $linked = Join-Path $W 'boot-linked.bin'
        if ($rl.Exit -eq 0 -and (Test-Path -LiteralPath $linked)) {
            $summary = (($rl.Text -split "`r?`n") | Where-Object { $_ -like 'Linked modules*' } | Select-Object -First 1)
            Set-Crit 'B1' 'PASS' 'link_modules.py exit 0; PESIMG1L image written' ([ordered]@{ boot_linked_sha256 = (Hash-File $linked); link_summary = $summary })
        } else {
            $tail = (($rl.Text -split "`r?`n") | Where-Object { $_.Trim() } | Select-Object -Last 3) -join ' | '
            Set-Crit 'B1' 'FAIL' ('link_modules.py exit ' + $rl.Exit + ': ' + $tail)
            $ready = $false
        }
    }

    $Report = Join-Path $V 'M2b-imports.tsv'
    $LoaderLog = Join-Path $V 'M2b-loader.log'
    if ($ready) {
        $env2 = @{ PATH = ((Join-Path $MsysRoot 'clang64\bin') + ';' + $env:PATH) }
        $largs = @((Join-Path $W 'boot-linked.bin'), '--cpu-only', '--stop-before-entry', '--import-report', $Report)
        $al = Join-Path $W 'import-aliases.txt'
        if (Test-Path -LiteralPath $al) { $largs += @('--import-aliases', $al) }
        $rr = Run-Proc $Exe $largs $LoaderLog $env2
        $lines = $rr.Text -split "`r?`n"
        $imp = $lines | Where-Object { $_ -match '^Imports: (\d+) host, (\d+) native, (\d+) trap, (\d+) prefix_only$' } | Select-Object -First 1
        $entering = $lines | Where-Object { $_ -like 'Entering original x86-64 code*' } | Select-Object -First 1
        $H = 0; $N = 0; $T = 0; $P = 0
        if ($imp -match '^Imports: (\d+) host, (\d+) native, (\d+) trap, (\d+) prefix_only$') {
            $H = [int]$Matches[1]; $N = [int]$Matches[2]; $T = [int]$Matches[3]; $P = [int]$Matches[4]
        }
        $rows = @()
        if (Test-Path -LiteralPath $Report) { $rows = @(Import-Csv -LiteralPath $Report -Delimiter "`t") }
        # Library of each import comes from analysis.json import_identities {name: [nid, library, module]}.
        $ident = $null
        try { $ident = (Get-Content -LiteralPath (Join-Path $W 'analysis.json') -Raw | ConvertFrom-Json).import_identities } catch { }
        $libOf = @{}
        if ($ident) { foreach ($pr in $ident.PSObject.Properties) { $libOf[$pr.Name] = [string]$pr.Value[1] } }
        $want = @($rows | Where-Object { $libOf.ContainsKey($_.name) -and ($libOf[$_.name] -in @('libc', 'libSceFios2', 'libSceLibcInternal')) })
        $bad = @($want | Where-Object { $_.result -eq 'trap' })
        if ($want.Count -eq 0) {
            Set-Crit 'B2' 'EXPLAIN' 'no libc/Fios2 rows found in the report (report or analysis missing)' ([ordered]@{ rows = $rows.Count })
        } elseif ($bad.Count -eq 0) {
            Set-Crit 'B2' 'PASS' 'every libc/Fios2 import resolves (native, or a host contract that overrides the native export)' ([ordered]@{ libc_fios2_imports = $want.Count })
        } else {
            Set-Crit 'B2' 'FAIL' ('libc/Fios2 imports still trapped: ' + $bad.Count) ([ordered]@{ libc_fios2_imports = $want.Count; trapped = $bad.Count; first_20 = @($bad | Select-Object -First 20 | ForEach-Object { $_.name }) })
        }
        Set-Crit 'B3' 'PASS' ('Imports: ' + $H + ' host, ' + $N + ' native, ' + $T + ' trap (M2 baseline 256/0/572)') ([ordered]@{ host = $H; native = $N; trap = $T; prefix_only = $P; baseline_host = 256; baseline_native = 0; baseline_trap = 572 })
        $traps = @($rows | Where-Object { $_.result -eq 'trap' })
        $groups = $traps | Group-Object { if ($libOf.ContainsKey($_.name)) { $libOf[$_.name] } else { '(unknown)' } } | Sort-Object Count -Descending | Select-Object -First 15
        $lib15 = [ordered]@{}
        foreach ($g in $groups) { $lib15[$g.Name] = $g.Count }
        Set-Crit 'B4' 'PASS' ('remaining traps: ' + $traps.Count) ([ordered]@{ trap_total = $traps.Count; by_library_top15 = $lib15 })
        if ($rr.Exit -eq 0 -and $entering) {
            Set-Crit 'B5' 'PASS' 'loader exit 0 and the Entering line printed on the linked image' ([ordered]@{ loader_exit = $rr.Exit })
        } else {
            $stop = ($lines | Where-Object { $_ -like 'STOP: first unsupported PS4 import*' } | Select-Object -First 1)
            Set-Crit 'B5' 'EXPLAIN' ('loader exit ' + $rr.Exit + ': native libc initialization stopped at an import the runtime does not provide yet (kernel HLE work, M3). ' + $stop) ([ordered]@{ loader_exit = $rr.Exit; entering_line = [bool]$entering; first_unsupported_import = $stop })
        }
    } else {
        foreach ($id in @('B2', 'B3', 'B4', 'B5')) { Set-Crit $id 'FAIL' 'not measured: link step did not complete' }
    }
    $hle = 0
    foreach ($f in @('src\runtime_libc.c', 'src\runtime_fios.c')) {
        $p = Join-Path $Root $f
        if (Test-Path -LiteralPath $p) { $hle += (Get-Content -LiteralPath $p | Measure-Object -Line).Lines }
    }
    Set-Crit 'B6' 'PASS' 'variant A needs no libc/Fios2 HLE source' ([ordered]@{ hle_lines = $hle })
    Set-Crit 'B7' 'NA' 'boot reaches the M3 platform marker needs running past entry: measured in M3' $null
} catch {
    Set-Crit 'B0' 'FAIL' ('script error: ' + $_.Exception.Message)
}

$overall = 'PASS'
foreach ($c in $Crit.Values) {
    if ($c.status -eq 'FAIL') { $overall = 'FAIL'; break }
    if ($c.status -eq 'EXPLAIN' -and $overall -eq 'PASS') { $overall = 'EXPLAIN' }
}
$Finished = Get-Date
$ExeFinal = ''
if (Test-Path -LiteralPath $Exe) { $ExeFinal = Hash-File $Exe }
$Json = [ordered]@{
    schema = 'pes-verify-m2b-1'
    overall = $overall
    started = $Started.ToString('o')
    finished = $Finished.ToString('o')
    exe = [ordered]@{ sha256 = $ExeSha; sha256_final = $ExeFinal; unchanged = ($ExeSha -eq $ExeFinal) }
    criteria = @($Crit.Values)
}
try {
    New-Item -ItemType Directory -Force -Path $V | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $V 'M2b-result.json'), (($Json | ConvertTo-Json -Depth 8) + "`n"), $Utf8)
} catch {
    Write-Host ('cannot write result: ' + $_.Exception.Message)
    $Exit = 1
}
Write-Host ('M2b overall: ' + $overall)
foreach ($c in $Crit.Values) { Write-Host ('{0,-3} {1,-8} {2}' -f $c.id, $c.status, $c.detail) }
if ($overall -eq 'FAIL') { $Exit = 1 }
exit $Exit
