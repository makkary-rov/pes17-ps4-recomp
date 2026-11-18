# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    M4 measurement: the title and main menu are drawn, and a menu soak runs without running out of GPU memory.
.DESCRIPTION
    Needs out\verify\m2b\boot-linked.bin (verify_M2b.ps1). Starts the game like verify_M3.ps1, takes screenshots
    of the primary screen at -ShotAt seconds, then lets it run until -Seconds (default 600 = 10-minute soak) and
    stops it. Writes out\verify\M4-result.json, out\verify\M4-run.log and out\verify\M4-shot-<n>.png (private).
    Criteria: D1 the game ran the whole soak (no stop, fault or exit), D2 no out-of-memory line in the log,
    D3 the best screenshot has a non-black pixel ratio of at least -MinNonBlack, D4 the user's sign-off text
    (-SignOff) is recorded. Leave the game window in front during the run.
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File verify\verify_M4.ps1 -Game <path to your game folder> -SignOff "title and menu drawn correctly"
#>
param(
    [Parameter(Mandatory = $true)][string]$Game,
    [int]$Seconds = 600,
    [int[]]$ShotAt = @(45, 90, 300),
    [double]$MinNonBlack = 0.25,
    [string]$SignOff = ''
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
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

function Set-Crit([string]$Id, [string]$Status, [string]$Detail, $Values = $null) {
    $Crit[$Id] = [ordered]@{ id = $Id; status = $Status; detail = $Detail; values = $Values }
}
function Hash-File([string]$P) { return (Get-FileHash -LiteralPath $P -Algorithm SHA256).Hash.ToLowerInvariant() }
# Screenshot of the primary screen; returns the ratio of pixels brighter than near-black (sampled every 4th pixel).
function Take-Shot([string]$Path) {
    $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
    $g.Dispose()
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $lit = 0; $total = 0
    for ($y = 0; $y -lt $bmp.Height; $y += 4) {
        for ($x = 0; $x -lt $bmp.Width; $x += 4) {
            $c = $bmp.GetPixel($x, $y)
            if (($c.R + $c.G + $c.B) -gt 45) { $lit++ }
            $total++
        }
    }
    $bmp.Dispose()
    if ($total -eq 0) { return 0.0 }
    return [math]::Round($lit / $total, 4)
}

$Exit = 0
$ExeSha = ''
try {
    New-Item -ItemType Directory -Force -Path $V | Out-Null
    $GameDir = $null
    try { $GameDir = (Resolve-Path -LiteralPath $Game -ErrorAction Stop).Path } catch { }
    $Image = Join-Path $W 'boot-linked.bin'
    $ready = $true
    if (-not $GameDir) { Set-Crit 'D1' 'FAIL' 'game directory not found'; $ready = $false }
    elseif (-not (Test-Path -LiteralPath $Image)) { Set-Crit 'D1' 'FAIL' 'out\verify\m2b\boot-linked.bin missing: run verify_M2b.ps1 first'; $ready = $false }
    elseif (-not (Test-Path -LiteralPath $Exe)) { Set-Crit 'D1' 'FAIL' 'out\pes17recomp.exe missing'; $ready = $false }

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
        $p = [System.Diagnostics.Process]::Start($psi)
        $p.StandardInput.Close()
        $so = $p.StandardOutput.ReadToEndAsync()
        $se = $p.StandardError.ReadToEndAsync()
        $shots = New-Object System.Collections.ArrayList
        $t0 = Get-Date
        $i = 0
        foreach ($at in ($ShotAt | Sort-Object)) {
            if ($at -ge $Seconds) { continue }
            $wait = $at - ((Get-Date) - $t0).TotalSeconds
            if ($wait -gt 0 -and $p.WaitForExit([int]($wait * 1000))) { break }
            if ($p.HasExited) { break }
            $i++
            $png = Join-Path $V ('M4-shot-' + $i + '.png')
            $ratio = Take-Shot $png
            [void]$shots.Add([ordered]@{ at_seconds = $at; non_black = $ratio; file = ('M4-shot-' + $i + '.png') })
        }
        $left = $Seconds - ((Get-Date) - $t0).TotalSeconds
        $alive = -not $p.HasExited
        if ($alive -and $left -gt 0) { $alive = -not $p.WaitForExit([int]($left * 1000)) }
        $killed = $false
        if ($alive) { try { $p.Kill(); $killed = $true } catch { } ; $p.WaitForExit(5000) | Out-Null }
        $code = $null
        if ($p.HasExited) { $code = $p.ExitCode }
        $ran = [math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
        $Text = $so.Result + "`n" + $se.Result
        [System.IO.File]::WriteAllText((Join-Path $V 'M4-run.log'), $Text, $Utf8)
        $lines = $Text -split "`r?`n"

        $stop = $lines | Where-Object { $_ -like 'STOP:*' } | Select-Object -First 1
        $fault = $lines | Where-Object { $_ -match '^(Guest|Host) fault ' } | Select-Object -First 1
        if ($killed -and -not $stop -and -not $fault) { Set-Crit 'D1' 'PASS' ('the game ran the whole soak (' + $Seconds + ' s) and was stopped by the script') ([ordered]@{ seconds_run = $ran }) }
        else { Set-Crit 'D1' 'FAIL' ('the run ended early after ' + $ran + ' s') ([ordered]@{ seconds_run = $ran; exit_code = $code; first_stop = $stop; first_fault = $fault }) }

        $oom = $lines | Where-Object { $_ -match '(?i)out of (device |host )?memory|ErrorOutOfDeviceMemory|ErrorOutOfHostMemory|\bOOM\b' } | Select-Object -First 3
        if (-not $oom) { Set-Crit 'D2' 'PASS' 'no out-of-memory line in the log' $null }
        else { Set-Crit 'D2' 'FAIL' 'out-of-memory lines in the log' ([ordered]@{ lines = @($oom) }) }

        $best = 0.0
        foreach ($s in $shots) { if ($s.non_black -gt $best) { $best = $s.non_black } }
        if ($shots.Count -eq 0) { Set-Crit 'D3' 'FAIL' 'no screenshot was taken (the game ended first)' $null }
        elseif ($best -ge $MinNonBlack) { Set-Crit 'D3' 'PASS' ('best screenshot non-black ratio ' + $best + ' >= ' + $MinNonBlack) ([ordered]@{ shots = @($shots) }) }
        else { Set-Crit 'D3' 'FAIL' ('best screenshot non-black ratio ' + $best + ' < ' + $MinNonBlack + ' (was the game window in front?)') ([ordered]@{ shots = @($shots) }) }
    } else {
        foreach ($id in @('D2', 'D3')) { Set-Crit $id 'FAIL' 'not measured: the run could not start' }
    }
    if ($SignOff.Trim()) { Set-Crit 'D4' 'PASS' 'user sign-off recorded' ([ordered]@{ text = $SignOff.Trim() }) }
    else { Set-Crit 'D4' 'EXPLAIN' 'no user sign-off given (-SignOff "...")' $null }
} catch {
    Set-Crit 'D0' 'FAIL' ('script error: ' + $_.Exception.Message)
}

$overall = 'PASS'
foreach ($c in $Crit.Values) {
    if ($c.status -eq 'FAIL') { $overall = 'FAIL'; break }
    if ($c.status -eq 'EXPLAIN' -and $overall -eq 'PASS') { $overall = 'EXPLAIN' }
}
$ExeFinal = ''
if (Test-Path -LiteralPath $Exe) { $ExeFinal = Hash-File $Exe }
$Json = [ordered]@{
    schema = 'pes-verify-m4-1'
    overall = $overall
    started = $Started.ToString('o')
    finished = (Get-Date).ToString('o')
    exe = [ordered]@{ sha256 = $ExeSha; sha256_final = $ExeFinal; unchanged = ($ExeSha -eq $ExeFinal) }
    criteria = @($Crit.Values)
}
try {
    [System.IO.File]::WriteAllText((Join-Path $V 'M4-result.json'), (($Json | ConvertTo-Json -Depth 8) + "`n"), $Utf8)
} catch {
    Write-Host ('cannot write result: ' + $_.Exception.Message)
    $Exit = 1
}
Write-Host ('M4 overall: ' + $overall)
foreach ($c in $Crit.Values) { Write-Host ('{0,-3} {1,-8} {2}' -f $c.id, $c.status, $c.detail) }
if ($overall -eq 'FAIL') { $Exit = 1 }
exit $Exit
