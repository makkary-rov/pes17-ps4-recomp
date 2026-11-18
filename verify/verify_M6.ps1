# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    M6 measurement: kickoff, then 5 minutes of live match without a fault, in 3 of 3 runs.
.DESCRIPTION
    Runs verify_M5.ps1 -Runs times (default 3) with -ReferenceSeconds 300. In each run YOU start a quick match
    from the menu and let it play; the script stops the game after -Seconds (default 600). Each run's log is kept
    as out\verify\M6-run<n>.log; out\verify\M6-result.json holds the per-run criteria. Criteria:
    K1 every run reached kickoff (observer mode=15 sub=2); K2 every run stayed live 300 s in one stretch;
    K3 no run had a stop, a fault or a self-ended process; K4 none of the three old post-kickoff fault signatures
    (guest 0x2ccec8 NULL read, guest 0x23a3dc0 write, C++ exception 0xE0424201) appears in any log;
    K5 the user's sign-off (-SignOff) is recorded.
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File verify\verify_M6.ps1 -Game <path to your game folder> -SignOff "3 matches past kickoff"
#>
param(
    [Parameter(Mandatory = $true)][string]$Game,
    [int]$Runs = 3,
    [int]$Seconds = 600,
    [string]$SignOff = ''
)

$ErrorActionPreference = 'Continue'
$Utf8 = New-Object System.Text.UTF8Encoding $false
$Started = Get-Date
$Root = Split-Path -Parent $PSScriptRoot
$V = Join-Path $Root 'out\verify'
$M5 = Join-Path $PSScriptRoot 'verify_M5.ps1'
$Crit = [ordered]@{}
function Set-Crit([string]$Id, [string]$Status, [string]$Detail, $Values = $null) {
    $Crit[$Id] = [ordered]@{ id = $Id; status = $Status; detail = $Detail; values = $Values }
}
$Signatures = @('guest\+0x2ccec8', 'guest\+0x23a3dc0', '(?i)0xe0424201')

$per = New-Object System.Collections.ArrayList
for ($i = 1; $i -le $Runs; $i++) {
    Write-Host ('M6 run ' + $i + ' of ' + $Runs + ': start a quick match and let it play.')
    # A crashed or unstarted run must not reuse the previous run's files.
    Remove-Item -LiteralPath (Join-Path $V 'M5-result.json'), (Join-Path $V 'M5-run.log') -ErrorAction SilentlyContinue
    & powershell -NoProfile -ExecutionPolicy Bypass -File $M5 -Game $Game -Seconds $Seconds -ReferenceSeconds 300 | Out-Host
    $res = $null; $log = Join-Path $V 'M5-run.log'; $kept = Join-Path $V ('M6-run' + $i + '.log')
    try { $res = Get-Content -Raw -LiteralPath (Join-Path $V 'M5-result.json') | ConvertFrom-Json } catch { }
    $hits = @(); $haveLog = $false
    if (Test-Path -LiteralPath $log) {
        Copy-Item -LiteralPath $log -Destination $kept -Force; $haveLog = $true
        foreach ($s in $Signatures) { if (Select-String -LiteralPath $kept -Pattern $s -Quiet) { $hits += $s } }
    }
    $st = @{}
    if ($res) { foreach ($c in $res.criteria) { $st[$c.id] = $c } }
    [void]$per.Add([ordered]@{ run = $i; kickoff = $st['E1'].status; live = $st['E2'].status; live_detail = $st['E2'].detail;
                               clean = $st['E3'].status; clean_detail = $st['E3'].detail; old_signatures = $hits; have_log = $haveLog; log = ('M6-run' + $i + '.log') })
}

function All-Pass($key) { return (@($per | Where-Object { $_[$key] -ne 'PASS' }).Count -eq 0 -and $per.Count -eq $Runs) }
if (All-Pass 'kickoff') { Set-Crit 'K1' 'PASS' ('kickoff observed in ' + $Runs + ' of ' + $Runs + ' runs') } else { Set-Crit 'K1' 'FAIL' 'kickoff missing in at least one run' }
if (All-Pass 'live') { Set-Crit 'K2' 'PASS' ('live 300 s in one stretch in ' + $Runs + ' of ' + $Runs + ' runs') } else { Set-Crit 'K2' 'FAIL' 'at least one run was not live 300 s in one stretch' }
if (All-Pass 'clean') { Set-Crit 'K3' 'PASS' 'no stop, fault or self-ended process in any run' } else { Set-Crit 'K3' 'FAIL' 'at least one run had a stop, a fault or ended by itself' }
if (@($per | Where-Object { -not $_.have_log }).Count -gt 0) { Set-Crit 'K4' 'FAIL' 'a run left no log: the old fault signatures could not be checked' }
elseif (@($per | Where-Object { $_.old_signatures.Count -gt 0 }).Count -eq 0) { Set-Crit 'K4' 'PASS' 'none of the three old post-kickoff fault signatures in any log' }
else { Set-Crit 'K4' 'FAIL' 'an old post-kickoff fault signature appeared' }
if ($SignOff.Trim()) { Set-Crit 'K5' 'PASS' 'user sign-off recorded' ([ordered]@{ text = $SignOff.Trim() }) }
else { Set-Crit 'K5' 'EXPLAIN' 'no user sign-off given (-SignOff "...")' }

$overall = 'PASS'
foreach ($c in $Crit.Values) {
    if ($c.status -eq 'FAIL') { $overall = 'FAIL'; break }
    if ($c.status -eq 'EXPLAIN' -and $overall -eq 'PASS') { $overall = 'EXPLAIN' }
}
$Json = [ordered]@{ schema = 'pes-verify-m6-1'; overall = $overall; started = $Started.ToString('o'); finished = (Get-Date).ToString('o');
                    runs = @($per); criteria = @($Crit.Values) }
[System.IO.File]::WriteAllText((Join-Path $V 'M6-result.json'), (($Json | ConvertTo-Json -Depth 8) + "`n"), $Utf8)
Write-Host ('M6 overall: ' + $overall)
foreach ($c in $Crit.Values) { Write-Host ('{0,-3} {1,-8} {2}' -f $c.id, $c.status, $c.detail) }
if ($overall -eq 'FAIL') { exit 1 }
exit 0
