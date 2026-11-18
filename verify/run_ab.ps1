# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    Visual A/B run: starts the game with extra environment settings and no measurement.
.DESCRIPTION
    Needs out\verify\m2b\boot-linked.bin (verify_M2b.ps1). Each -Env entry is NAME=VALUE. The run ends when you
    close the game; its log goes to out\verify\AB-run.log (private). Several settings: separate them with ';'
    (powershell -File passes a list as one string). -Capture records frames (vk_frame_capture.h): press Enter in
    this window while the scene is on screen; each Enter writes out\verify\capture\frame_<time>.txt.
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File verify\run_ab.ps1 -Game <path to your game folder> -Env PES_MANUAL_INTERP=0
#>
param(
    [Parameter(Mandatory = $true)][string]$Game,
    [string[]]$Env = @(),
    [switch]$Capture
)
$Root = Split-Path -Parent $PSScriptRoot
$V = Join-Path $Root 'out\verify'
$W = Join-Path $V 'm2b'
$MsysRoot = 'C:\msys64'
if ($env:MSYS2_ROOT) { $MsysRoot = $env:MSYS2_ROOT }
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = Join-Path $Root 'out\pes17recomp.exe'
$args0 = '"' + (Join-Path $W 'boot-linked.bin') + '" --app0 "' + (Resolve-Path -LiteralPath $Game).Path + '"'
$Aliases = Join-Path $W 'import-aliases.txt'
if (Test-Path -LiteralPath $Aliases) { $args0 += ' --import-aliases "' + $Aliases + '"' }
$Content = Join-Path $V 'run1\content.bin'
if (Test-Path -LiteralPath $Content) { $args0 += ' --content-profile "' + $Content + '"' }
$psi.Arguments = $args0
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.RedirectStandardInput = $true
$psi.WorkingDirectory = $Root
$psi.EnvironmentVariables['PATH'] = (Join-Path $MsysRoot 'clang64\bin') + ';' + $env:PATH
$Trigger = Join-Path $V 'capture.now'
if ($Capture) {
    New-Item -ItemType Directory -Force -Path (Join-Path $V 'capture') | Out-Null
    Remove-Item -LiteralPath $Trigger -ErrorAction SilentlyContinue
    $Env += @('PES_CAPTURE_TRIGGER=' + $Trigger, 'PES_CAPTURE_DIR=' + (Join-Path $V 'capture'))
}
$Env = @($Env | ForEach-Object { $_ -split ';' } | Where-Object { $_.Trim() })
foreach ($e in $Env) {
    $k, $v = $e -split '=', 2
    $psi.EnvironmentVariables[$k] = $v
    Write-Host ('A/B setting: ' + $k + '=' + $v)
}
$p = [System.Diagnostics.Process]::Start($psi)
$p.StandardInput.Close()
$so = $p.StandardOutput.ReadToEndAsync()
$se = $p.StandardError.ReadToEndAsync()
if ($Capture) { Write-Host 'Press Enter here to capture the frame on screen (any number of times).' }
while (-not $p.WaitForExit(200)) {
    if ($Capture -and [Console]::KeyAvailable -and [Console]::ReadKey($true).Key -eq 'Enter') {
        New-Item -ItemType File -Force -Path $Trigger | Out-Null
        Write-Host ('capture requested at ' + (Get-Date).ToString('T'))
    }
}
[System.IO.File]::WriteAllText((Join-Path $V 'AB-run.log'), ('A/B: ' + ($Env -join ' ') + "`n" + $so.Result + "`n" + $se.Result), (New-Object System.Text.UTF8Encoding $false))
Write-Host ('exit ' + $p.ExitCode + '; log out\verify\AB-run.log')
