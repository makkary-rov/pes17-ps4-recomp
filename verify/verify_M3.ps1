# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    M3 measurement: run the game on the natively linked image past the entry point and record how far it gets.
.DESCRIPTION
    Needs out\verify\m2b\boot-linked.bin and import-aliases.txt (run verify_M2b.ps1 first). Starts
    out\pes17recomp.exe on the linked image with the game directory as /app0, lets it run for -Seconds
    (default 60), then stops it, and writes out\verify\M3-result.json plus out\verify\M3-run.log.
    The result holds counts, the first unsupported import or fault line, and the platform marker
    (sceSaveDataMount of SYSTEM00000000, plan M3 marker 1). It holds no game bytes. The game directory
    is only read; save data goes to <repo>\user.
.PARAMETER Game
    The game directory.
.PARAMETER Seconds
    How long the game may run before it is stopped. Default 60.
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File verify\verify_M3.ps1 -Game <path to your game folder>
#>
param(
    [Parameter(Mandatory = $true)][string]$Game,
    [int]$Seconds = 60
)

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

$Exit = 0
$ExeSha = ''
$Text = ''
try {
    New-Item -ItemType Directory -Force -Path $V | Out-Null
    $GameDir = $null
    try { $GameDir = (Resolve-Path -LiteralPath $Game -ErrorAction Stop).Path } catch { }
    $Image = Join-Path $W 'boot-linked.bin'
    $Aliases = Join-Path $W 'import-aliases.txt'
    $ready = $true
    if (-not $GameDir) { Set-Crit 'M1' 'FAIL' 'game directory not found'; $ready = $false }
    elseif (-not (Test-Path -LiteralPath $Image)) { Set-Crit 'M1' 'FAIL' 'out\verify\m2b\boot-linked.bin missing: run verify_M2b.ps1 first'; $ready = $false }
    elseif (-not (Test-Path -LiteralPath $Exe)) { Set-Crit 'M1' 'FAIL' 'out\pes17recomp.exe missing'; $ready = $false }

    if ($ready) {
        $ExeSha = Hash-File $Exe
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $Exe
        $argText = '"' + $Image + '" --app0 "' + $GameDir + '"'
        if (Test-Path -LiteralPath $Aliases) { $argText += ' --import-aliases "' + $Aliases + '"' }
        # The AppContent profile (SKU and add-ons) that verify_M2.ps1 generates from the game's param.sfo.
        $Content = Join-Path $V 'run1\content.bin'
        if (Test-Path -LiteralPath $Content) { $argText += ' --content-profile "' + $Content + '"' }
        $psi.Arguments = $argText
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.RedirectStandardInput = $true
        $psi.WorkingDirectory = $Root
        $psi.EnvironmentVariables['PATH'] = (Join-Path $MsysRoot 'clang64\bin') + ';' + $env:PATH
        $psi.EnvironmentVariables['PES_TRACE'] = 'loader,kernel,title'
        $psi.EnvironmentVariables['PES_TRACE_CAP'] = '2000'
        $p = [System.Diagnostics.Process]::Start($psi)
        $p.StandardInput.Close()
        $so = $p.StandardOutput.ReadToEndAsync()
        $se = $p.StandardError.ReadToEndAsync()
        $finishedInTime = $p.WaitForExit($Seconds * 1000)
        $killed = $false
        if (-not $finishedInTime) { try { $p.Kill(); $killed = $true } catch { } ; $p.WaitForExit(5000) | Out-Null }
        $code = $null
        if ($p.HasExited) { $code = $p.ExitCode }
        $Text = $so.Result + "`n" + $se.Result
        [System.IO.File]::WriteAllText((Join-Path $V 'M3-run.log'), $Text, $Utf8)
        $lines = $Text -split "`r?`n"

        $enter = $lines | Where-Object { $_ -like 'Entering original x86-64 code*' } | Select-Object -First 1
        if ($enter) { Set-Crit 'M1' 'PASS' 'linked modules initialized and the loader entered the game code' ([ordered]@{ seconds_run = $Seconds; exit_code = $code; stopped_by_script = $killed }) }
        else { Set-Crit 'M1' 'FAIL' ('the loader did not reach the game code; exit ' + $code) ([ordered]@{ exit_code = $code }) }

        $stop = $lines | Where-Object { $_ -like 'STOP:*' } | Select-Object -First 1
        $fault = $lines | Where-Object { $_ -match '^(Guest|Host) fault ' } | Select-Object -First 1
        if ($stop -or $fault) {
            Set-Crit 'M2' 'EXPLAIN' 'the run ended at a stop or fault (next kernel/runtime work item)' ([ordered]@{ first_stop = $stop; first_fault = $fault; exit_code = $code })
        } elseif ($killed) {
            Set-Crit 'M2' 'PASS' ('no stop or fault in ' + $Seconds + ' seconds; the game was still running') ([ordered]@{ exit_code = $code })
        } else {
            Set-Crit 'M2' 'EXPLAIN' ('the process exited by itself with code ' + $code) ([ordered]@{ exit_code = $code })
        }

        $mount = $lines | Where-Object { $_ -match "sceSaveDataMount\(.*dir='SYSTEM00000000'" } | Select-Object -First 1
        if ($mount) { Set-Crit 'M3' 'PASS' 'platform marker reached: sceSaveDataMount for SYSTEM00000000' ([ordered]@{ line = $mount }) }
        else { Set-Crit 'M3' 'EXPLAIN' 'platform marker (sceSaveDataMount SYSTEM00000000) not reached yet' $null }

        $api = $lines | Where-Object { $_ -like 'API:*' -or $_ -like 'Caller*' } | Select-Object -First 3
        $mounts = ($lines | Where-Object { $_ -like 'Runtime: sceSaveDataMount(*' }).Count
        $threads = ($lines | Where-Object { $_ -like "Runtime: guest thread '*created*" }).Count
        $files = ($lines | Where-Object { $_ -like 'Runtime: files opened=*' } | Select-Object -Last 1)
        Set-Crit 'M4' 'PASS' 'activity counts' ([ordered]@{ save_mounts = $mounts; guest_threads_created = $threads; file_summary = $files; stop_context = @($api) })
        Set-Crit 'M5' 'NA' 'ExitWait1 return and the save round trip are measured when the platform marker is reached' $null
    } else {
        foreach ($id in @('M2', 'M3', 'M4', 'M5')) { Set-Crit $id 'FAIL' 'not measured: the run could not start' }
    }
} catch {
    Set-Crit 'M0' 'FAIL' ('script error: ' + $_.Exception.Message)
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
    schema = 'pes-verify-m3-1'
    overall = $overall
    started = $Started.ToString('o')
    finished = $Finished.ToString('o')
    exe = [ordered]@{ sha256 = $ExeSha; sha256_final = $ExeFinal; unchanged = ($ExeSha -eq $ExeFinal) }
    criteria = @($Crit.Values)
}
try {
    [System.IO.File]::WriteAllText((Join-Path $V 'M3-result.json'), (($Json | ConvertTo-Json -Depth 8) + "`n"), $Utf8)
} catch {
    Write-Host ('cannot write result: ' + $_.Exception.Message)
    $Exit = 1
}
Write-Host ('M3 overall: ' + $overall)
foreach ($c in $Crit.Values) { Write-Host ('{0,-3} {1,-8} {2}' -f $c.id, $c.status, $c.detail) }
if ($overall -eq 'FAIL') { $Exit = 1 }
exit $Exit
