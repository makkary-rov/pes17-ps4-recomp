# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    M5 measurement: a live match. The read-only state observer (G-44) must report mode=15 sub=2 (match running)
    and the match must stay live at least until the old reference (about 109.4 s after start).
.DESCRIPTION
    Needs out\verify\m2b\boot-linked.bin (verify_M2b.ps1). Starts the game; YOU start a match from the menu with
    the keyboard (any quick exhibition match). The game runs until -Seconds (default 300) or until you close it.
    Writes out\verify\M5-result.json and out\verify\M5-run.log. Criteria:
    E1 observer reported mode=15 sub=2; E2 the last observer sample in mode 15 is at or after -ReferenceSeconds;
    E3 no stop or fault before that sample; E4 the user's sign-off (-SignOff) is recorded.
    -DumpAt 120,180 asks the runtime for a thread dump (every thread's guest call chain) at those seconds.
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File verify\verify_M5.ps1 -Game <path to your game folder> -SignOff "played a live match"
#>
param(
    [Parameter(Mandatory = $true)][string]$Game,
    [int]$Seconds = 300,
    [double]$ReferenceSeconds = 109.4,
    [string]$SignOff = '',
    [switch]$FileTrace,
    [int[]]$DumpAt = @(150, 250)
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
try {
    New-Item -ItemType Directory -Force -Path $V | Out-Null
    $GameDir = $null
    try { $GameDir = (Resolve-Path -LiteralPath $Game -ErrorAction Stop).Path } catch { }
    $Image = Join-Path $W 'boot-linked.bin'
    $ready = $true
    if (-not $GameDir) { Set-Crit 'E1' 'FAIL' 'game directory not found'; $ready = $false }
    elseif (-not (Test-Path -LiteralPath $Image)) { Set-Crit 'E1' 'FAIL' 'out\verify\m2b\boot-linked.bin missing: run verify_M2b.ps1 first'; $ready = $false }
    elseif (-not (Test-Path -LiteralPath $Exe)) { Set-Crit 'E1' 'FAIL' 'out\pes17recomp.exe missing'; $ready = $false }

    if ($ready) {
        $ExeSha = Hash-File $Exe
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $Exe
        $argText = '"' + $Image + '" --app0 "' + $GameDir + '"'
        $Aliases = Join-Path $W 'import-aliases.txt'
        if (Test-Path -LiteralPath $Aliases) { $argText += ' --import-aliases "' + $Aliases + '"' }
        $Content = Join-Path $V 'run1\content.bin'
        if (Test-Path -LiteralPath $Content) { $argText += ' --content-profile "' + $Content + '"' }
        $psi.Arguments = $argText
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.RedirectStandardInput = $true
        $psi.WorkingDirectory = $Root
        $psi.EnvironmentVariables['PATH'] = (Join-Path $MsysRoot 'clang64\bin') + ';' + $env:PATH
        if ($FileTrace) { $psi.EnvironmentVariables['PES_FILE_TRACE'] = '1' }  # every open/read/stat goes to the log
        $p = [System.Diagnostics.Process]::Start($psi)
        $p.StandardInput.Close()
        $so = $p.StandardOutput.ReadToEndAsync()
        $se = $p.StandardError.ReadToEndAsync()
        $killed = $false
        $t0 = Get-Date
        $DumpNotes = New-Object System.Collections.ArrayList
        foreach ($at in ($DumpAt | Sort-Object)) {  # the runtime's dump thread waits on Local\pes-dump-<pid>
            if ($at -ge $Seconds) { continue }
            $wait = $at - ((Get-Date) - $t0).TotalSeconds
            if ($wait -gt 0 -and $p.WaitForExit([int]($wait * 1000))) { break }
            $note = 'DumpAt ' + $at + ' s: '
            try { $ev = [System.Threading.EventWaitHandle]::OpenExisting('Local\pes-dump-' + $p.Id); $note += 'set ' + $ev.Set(); $ev.Dispose() } catch { $note += 'failed: ' + $_.Exception.Message }
            [void]$DumpNotes.Add($note)
        }
        $left = [math]::Max(0, $Seconds - ((Get-Date) - $t0).TotalSeconds)
        if (-not $p.HasExited -and -not $p.WaitForExit([int]($left * 1000))) { try { $p.Kill(); $killed = $true } catch { } ; $p.WaitForExit(5000) | Out-Null }
        $code = $null
        if ($p.HasExited) { $code = $p.ExitCode }
        $Text = $so.Result + "`n" + $se.Result + "`n" + ($DumpNotes -join "`n")
        [System.IO.File]::WriteAllText((Join-Path $V 'M5-run.log'), $Text, $Utf8)
        $lines = $Text -split "`r?`n"

        $samples = @()
        foreach ($l in $lines) {
            if ($l -match '^Observer: t=([0-9.]+) mode=(\d+) sub=(\d+)$') {
                $samples += [pscustomobject]@{ t = [double]$Matches[1]; mode = [int]$Matches[2]; sub = [int]$Matches[3] }
            }
        }
        $live = @($samples | Where-Object { $_.mode -eq 15 -and $_.sub -eq 2 })
        $inMatch = @($samples | Where-Object { $_.mode -eq 15 })
        $modes = @($samples | ForEach-Object { '' + $_.mode + '/' + $_.sub } | Select-Object -Unique)
        if ($live.Count -gt 0) { Set-Crit 'E1' 'PASS' ('observer reported mode=15 sub=2 first at t=' + $live[0].t + ' s') ([ordered]@{ samples = $samples.Count; mode_sub_seen = $modes }) }
        elseif ($samples.Count -eq 0) { Set-Crit 'E1' 'FAIL' 'no observer line (the match controller was never created)' ([ordered]@{ exit_code = $code }) }
        else { Set-Crit 'E1' 'FAIL' 'observer never reported mode=15 sub=2 (no live match)' ([ordered]@{ mode_sub_seen = $modes }) }

        $lastMatch = 0.0
        if ($inMatch.Count -gt 0) { $lastMatch = ($inMatch | Measure-Object -Property t -Maximum).Maximum }
        if ($lastMatch -ge $ReferenceSeconds) { Set-Crit 'E2' 'PASS' ('last mode-15 sample at t=' + $lastMatch + ' s >= reference ' + $ReferenceSeconds + ' s') $null }
        else { Set-Crit 'E2' 'FAIL' ('last mode-15 sample at t=' + $lastMatch + ' s < reference ' + $ReferenceSeconds + ' s') $null }

        $stop = $lines | Where-Object { $_ -like 'STOP:*' } | Select-Object -First 1
        $fault = $lines | Where-Object { $_ -match '^(Guest|Host) fault ' } | Select-Object -First 1
        if (-not $stop -and -not $fault) { Set-Crit 'E3' 'PASS' 'no stop or fault in the run' ([ordered]@{ exit_code = $code; stopped_by_script = $killed }) }
        else { Set-Crit 'E3' 'EXPLAIN' 'the run ended at a stop or fault (see M5-run.log; the post-kickoff faults are M6 work)' ([ordered]@{ first_stop = $stop; first_fault = $fault; exit_code = $code }) }
    } else {
        foreach ($id in @('E2', 'E3')) { Set-Crit $id 'FAIL' 'not measured: the run could not start' }
    }
    if ($SignOff.Trim()) { Set-Crit 'E4' 'PASS' 'user sign-off recorded' ([ordered]@{ text = $SignOff.Trim() }) }
    else { Set-Crit 'E4' 'EXPLAIN' 'no user sign-off given (-SignOff "...")' $null }
} catch {
    Set-Crit 'E0' 'FAIL' ('script error: ' + $_.Exception.Message)
}

$overall = 'PASS'
foreach ($c in $Crit.Values) {
    if ($c.status -eq 'FAIL') { $overall = 'FAIL'; break }
    if ($c.status -eq 'EXPLAIN' -and $overall -eq 'PASS') { $overall = 'EXPLAIN' }
}
$ExeFinal = ''
if (Test-Path -LiteralPath $Exe) { $ExeFinal = Hash-File $Exe }
$Json = [ordered]@{
    schema = 'pes-verify-m5-1'
    overall = $overall
    started = $Started.ToString('o')
    finished = (Get-Date).ToString('o')
    exe = [ordered]@{ sha256 = $ExeSha; sha256_final = $ExeFinal; unchanged = ($ExeSha -eq $ExeFinal) }
    criteria = @($Crit.Values)
}
try {
    [System.IO.File]::WriteAllText((Join-Path $V 'M5-result.json'), (($Json | ConvertTo-Json -Depth 8) + "`n"), $Utf8)
} catch {
    Write-Host ('cannot write result: ' + $_.Exception.Message)
    $Exit = 1
}
Write-Host ('M5 overall: ' + $overall)
foreach ($c in $Crit.Values) { Write-Host ('{0,-3} {1,-8} {2}' -f $c.id, $c.status, $c.detail) }
if ($overall -eq 'FAIL') { $Exit = 1 }
exit $Exit
