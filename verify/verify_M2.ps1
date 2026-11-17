# SPDX-License-Identifier: GPL-2.0-or-later
<#
.SYNOPSIS
    M2 verification for one PES 2017 game directory: criteria C1-C9 (.agent-work spec.md, section 1).
.DESCRIPTION
    Runs, in order: build.sh in the MSYS2 CLANG64 shell (skipped by -SkipBuild); prepare.py twice (C1);
    content_profile.py; the loader with --stop-before-entry, the import report and an image dump
    (C2, C3, C5, C6); relocation and title checks (C4, C8); the GNM table generator (C7a; C7b is N/A
    on Windows); and the agent-side gate (C9). Each step is logged and a failing step does not stop
    the rest. out\verify\M2-result.json is written in every case.
    The result file holds hashes, counts, ids, CPU and tool names, and the first import names of a
    failed C5 only. It holds no game bytes.
.PARAMETER Game
    The game directory: the one with eboot.elf (or eboot.bin) and sce_sys\param.sfo.
.PARAMETER SkipBuild
    Do not run build.sh; use the existing out\pes17recomp.exe.
.PARAMETER TitleConfig
    Title config (schema pes-title-1). Relative paths are taken from the repo root.
    Default: src\title\CUSA04957.json.
.PARAMETER AllowHashMismatch
    Pass --allow-hash-mismatch to prepare.py. The mismatch is recorded in analysis.json.
.PARAMETER Out
    Output root. Artifacts go to <Out>\verify. Relative paths are taken from the repo root.
    Default: out. The loader is always out\pes17recomp.exe in the repo root (build.sh output).
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File verify\verify_M2.ps1 -Game <path to your game folder>
#>
param(
    [Parameter(Mandatory = $true)][string]$Game,
    [switch]$SkipBuild,
    [string]$TitleConfig = 'src\title\CUSA04957.json',
    [switch]$AllowHashMismatch,
    [string]$Out = 'out'
)

$ErrorActionPreference = 'Continue'
$Utf8 = New-Object System.Text.UTF8Encoding $false
$StartedAt = Get-Date
$Root = Split-Path -Parent $PSScriptRoot
$ArgErrors = New-Object System.Collections.ArrayList
# Illegal path characters make GetFullPath throw. Record the error, use the fallback, and let the
# try block write it as a FAIL step, so the result JSON is still written.
function Resolve-Arg([string]$Base, [string]$Value, [string]$Label, [string]$Fallback) {
    try { return [IO.Path]::GetFullPath([IO.Path]::Combine($Base, $Value)) }
    catch { [void]$ArgErrors.Add($Label + ' has illegal path characters: ' + $Value); return $Fallback }
}
# Absolute, lower-case, no trailing separator, for comparing paths printed by the loader; '' when it cannot be resolved.
function Norm-Path([string]$P) {
    try { return ([IO.Path]::GetFullPath($P)).TrimEnd('\').ToLowerInvariant() } catch { return '' }
}
$GameDir = Resolve-Arg (Get-Location).Path $Game '-Game' $Game
$TitlePath = Resolve-Arg $Root $TitleConfig '-TitleConfig' $TitleConfig
$OutRoot = Resolve-Arg $Root $Out '-Out' (Join-Path $Root 'out')
$V = Join-Path $OutRoot 'verify'
New-Item -ItemType Directory -Force -Path $V | Out-Null
$Exe = Join-Path $Root 'out\pes17recomp.exe'
$ScriptLog = Join-Path $V 'M2-script.log'
$ResultPath = Join-Path $V 'M2-result.json'
$GnmDir = Join-Path $V 'gnm'
$DumpPath = Join-Path $V 'M2-mapped.bin'
$TsvPath = Join-Path $V 'M2-imports.tsv'
$LoaderLog = Join-Path $V 'M2-loader.log'
$Run1 = Join-Path $V 'run1'
$Run2 = Join-Path $V 'run2'
$GenOut = Join-Path $Root 'src\title\generated\pes_gnm_table.inc'

function Test-Quiet([string]$Path, [string]$Type) {
    try { return [bool](Test-Path -LiteralPath $Path -PathType $Type) } catch { return $false }
}

function Log([string]$Message) {
    $line = '{0} {1}' -f (Get-Date -Format 'HH:mm:ss'), $Message
    Write-Host $line
    try { [IO.File]::AppendAllText($ScriptLog, $line + "`r`n", $Utf8) } catch { }
}

function Save-Text([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, $Utf8)
}

function Quote-Arg([string]$Value) {
    if ($Value -eq '') { return '""' }
    if ($Value -match '[\s"]') { return '"' + $Value.Replace('"', '\"') + '"' }
    return $Value
}

function Run-Native {
    param([string]$Exe, [string[]]$ArgList, [string]$LogFile, [int]$TimeoutMs = 600000,
          [string]$StdoutFile = '', [hashtable]$EnvVars = @{}, [string]$WorkDir = $Root)
    $res = [pscustomobject]@{ Exit = -1; TimedOut = $false; Seconds = 0.0; Error = ''; Stdout = ''; Stderr = '' }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    try {
        $psi = New-Object Diagnostics.ProcessStartInfo
        $psi.FileName = $Exe
        $psi.Arguments = (@($ArgList | ForEach-Object { Quote-Arg ([string]$_) }) -join ' ')
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.RedirectStandardInput = $true
        $psi.CreateNoWindow = $true
        $psi.WorkingDirectory = $WorkDir
        foreach ($k in $EnvVars.Keys) { $psi.EnvironmentVariables[$k] = [string]$EnvVars[$k] }
        $p = New-Object Diagnostics.Process
        $p.StartInfo = $psi
        [void]$p.Start()
        $p.StandardInput.Close()
        $outTask = $p.StandardOutput.ReadToEndAsync()
        $errTask = $p.StandardError.ReadToEndAsync()
        if ($p.WaitForExit($TimeoutMs)) {
            $res.Exit = $p.ExitCode
        } else {
            $res.TimedOut = $true
            try { $p.Kill() } catch { }
            $p.WaitForExit()
        }
        $res.Stdout = $outTask.Result
        $res.Stderr = $errTask.Result
    } catch {
        $res.Error = $_.Exception.Message
    }
    $sw.Stop()
    $res.Seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1)
    if ($LogFile) {
        $logText = $res.Stdout + $res.Stderr
        if ($StdoutFile) { $logText = $res.Stderr }
        try { Save-Text $LogFile $logText } catch { $res.Error = $res.Error + ' (log write failed)' }
    }
    if ($StdoutFile) {
        try { Save-Text $StdoutFile $res.Stdout } catch { $res.Error = $res.Error + ' (stdout file write failed)' }
    }
    return $res
}

function Hash-File([string]$Path, [string]$Algorithm = 'SHA256') {
    return (Get-FileHash -LiteralPath $Path -Algorithm $Algorithm).Hash.ToLowerInvariant()
}

function Read-Utf8([string]$Path) {
    return [IO.File]::ReadAllText($Path, $Utf8)
}

function Last-Line([string]$Text) {
    $lines = @($Text -split "`r?`n" | Where-Object { $_.Trim() -ne '' })
    if ($lines.Count -eq 0) { return '(no output)' }
    $t = $lines[$lines.Count - 1].Trim()
    if ($t.Length -gt 200) { return $t.Substring(0, 200) }
    return $t
}

function Parse-Hex([string]$Text) {
    return [Convert]::ToInt64(($Text -replace '^0[xX]', ''), 16)
}

$Steps = New-Object System.Collections.ArrayList
function Add-Step([string]$Name, [string]$Status, [string]$Detail, $Seconds = 0, $Exit = $null) {
    [void]$Steps.Add([pscustomobject]@{ step = $Name; status = $Status; exit = $Exit; seconds = $Seconds; detail = $Detail })
    Log ('step {0}: {1} {2}' -f $Name, $Status, $Detail)
}

$CritIds = @('C1', 'C2', 'C3', 'C4', 'C5', 'C6', 'C7a', 'C7b', 'C8', 'C9')
$Crit = [ordered]@{}
foreach ($id in $CritIds) {
    $Crit[$id] = [pscustomobject]@{ id = $id; status = 'FAIL'; detail = 'not measured (script stopped before this criterion)'; values = [ordered]@{} }
}
function Set-Crit([string]$Id, [string]$Status, [string]$Detail, $Values = $null) {
    if ($null -eq $Values) { $Values = [ordered]@{} }
    $Crit[$Id] = [pscustomobject]@{ id = $Id; status = $Status; detail = $Detail; values = $Values }
}

# Python 3: py -3, python, or python3 (the first that reports Python 3).
$Py = $null
$PyVersion = 'not found'
foreach ($cand in @(@{ exe = 'python'; pre = @() }, @{ exe = 'py'; pre = @('-3') }, @{ exe = 'python3'; pre = @() })) {
    $r = Run-Native -Exe $cand.exe -ArgList (@($cand.pre) + @('--version'))
    $text = $r.Stdout + $r.Stderr
    if ($r.Exit -eq 0 -and $text -match 'Python 3\.') {
        $Py = $cand
        $PyVersion = $text.Trim()
        break
    }
}

function Run-Py([string]$Script, [string[]]$Rest, [string]$LogFile, [int]$TimeoutMs = 900000, [string]$StdoutFile = '') {
    if ($null -eq $Py) {
        $msg = 'python 3 not found (tried python, py -3, python3)'
        if ($LogFile) { try { Save-Text $LogFile $msg } catch { } }
        return [pscustomobject]@{ Exit = -1; TimedOut = $false; Seconds = 0; Error = $msg; Stdout = ''; Stderr = $msg }
    }
    return Run-Native -Exe $Py.exe -ArgList (@($Py.pre) + @($Script) + @($Rest)) -LogFile $LogFile -TimeoutMs $TimeoutMs -StdoutFile $StdoutFile
}

# MSYS2: C:\msys64 or MSYS2_ROOT. The CLANG64 environment is set the way the spec's Shell rule does.
$MsysRoot = 'C:\msys64'
if ($env:MSYS2_ROOT) { $MsysRoot = $env:MSYS2_ROOT }
$Bash = Join-Path $MsysRoot 'usr\bin\bash.exe'
$HasMsys = Test-Path -LiteralPath $Bash
$PosixRepo = $Root.Replace('\', '/')
$RootMatch = [regex]::Match($Root, '^([A-Za-z]):\\(.*)$')
if ($RootMatch.Success) {
    $PosixRepo = '/' + $RootMatch.Groups[1].Value.ToLowerInvariant() + '/' + $RootMatch.Groups[2].Value.Replace('\', '/')
}

function Run-Msys([string]$Command, [string]$LogFile, [int]$TimeoutMs = 3600000) {
    # stdin of every MSYS2 command is /dev/null: with no console stdin, Windows Python raises WinError 6 in subprocess.
    return Run-Native -Exe $Bash -ArgList @('-lc', ("cd '" + $PosixRepo + "' && ( " + $Command + " ) < /dev/null")) -LogFile $LogFile -TimeoutMs $TimeoutMs -EnvVars @{ MSYSTEM = 'CLANG64'; CHERE_INVOKING = '1' }
}

# objdump for the region disassembly must be GNU objdump (-b binary, --insn-width, GNU Intel syntax as
# gnm_sigs.py parses it). Candidates: the MSYS2 usr and CLANG64 bin folders, then objdump on PATH.
# LLVM objdump (clang64\bin\objdump.exe on this MSYS2 install) is refused: it has no -b binary.
$ObjdumpExe = $null
$ObjdumpVersion = 'not found'
$ObjdumpNote = ''
$ObjdumpCandidates = @((Join-Path $MsysRoot 'usr\bin\objdump.exe'), (Join-Path $MsysRoot 'clang64\bin\objdump.exe'), (Join-Path $MsysRoot 'mingw64\bin\objdump.exe'), (Join-Path $MsysRoot 'ucrt64\bin\objdump.exe'))
$cmdObj = Get-Command objdump -ErrorAction SilentlyContinue
if ($cmdObj) { $ObjdumpCandidates += $cmdObj.Source }
foreach ($c in $ObjdumpCandidates) {
    if ($ObjdumpExe -or -not (Test-Path -LiteralPath $c)) { continue }
    $ro = Run-Native -Exe $c -ArgList @('--version')
    $roLine = (($ro.Stdout -split "`r?`n")[0]).Trim()
    if ($roLine -match 'GNU objdump') {
        $ObjdumpExe = $c
        $ObjdumpVersion = $roLine
    } elseif (-not $ObjdumpNote) {
        $ObjdumpNote = ('found ' + $roLine + ' at ' + $c + ' (not GNU objdump; -b binary unsupported)')
    }
}
if (-not $ObjdumpExe -and -not $ObjdumpNote) { $ObjdumpNote = 'no objdump found' }
if (-not $ObjdumpExe) { $ObjdumpVersion = $ObjdumpNote }
# The loader needs the CLANG64 DLLs on PATH (STATUS_DLL_NOT_FOUND otherwise).
$LoaderEnv = @{}
if ($HasMsys) { $LoaderEnv['PATH'] = (Join-Path $MsysRoot 'clang64\bin') + ';' + $env:PATH }

$CpuName = 'unknown'
try { $CpuName = (Get-CimInstance -ClassName Win32_Processor | Select-Object -First 1).Name } catch { }
$OsName = 'unknown'
try {
    $osInfo = Get-CimInstance -ClassName Win32_OperatingSystem
    $OsName = ('{0} {1}' -f $osInfo.Caption, $osInfo.Version)
} catch { }

$Result = $null
$ExitCode = 0
try {
    foreach ($argErr in $ArgErrors) { Add-Step 'arguments' 'FAIL' $argErr }
    Log ('start: game={0} title={1} out={2}' -f $GameDir, $TitlePath, $V)
    Log ('python: {0}; objdump: {1}; msys: {2}' -f $PyVersion, $ObjdumpVersion, $HasMsys)

    # Inputs (hashes only).
    $GameOk = Test-Quiet $GameDir 'Container'
    if (-not $GameOk) { Add-Step 'game' 'FAIL' 'game directory not found' }
    $InputName = ''
    $InputMd5 = ''
    $InputSha = ''
    $inputPath = ''
    $elfIn = Join-Path $GameDir 'eboot.elf'
    $binIn = Join-Path $GameDir 'eboot.bin'
    if (Test-Quiet $elfIn 'Leaf') { $inputPath = $elfIn } elseif (Test-Quiet $binIn 'Leaf') { $inputPath = $binIn }
    if ($inputPath) {
        $InputName = Split-Path -Leaf $inputPath
        $InputMd5 = Hash-File $inputPath 'MD5'
        $InputSha = Hash-File $inputPath 'SHA256'
    }
    $SfoSha = ''
    $sfoPath = Join-Path $GameDir 'sce_sys\param.sfo'
    if (Test-Quiet $sfoPath 'Leaf') { $SfoSha = Hash-File $sfoPath }

    # Title config.
    $Cfg = $null
    $CfgOk = $false
    $CfgSha = ''
    try {
        $Cfg = (Read-Utf8 $TitlePath) | ConvertFrom-Json
        $CfgSha = Hash-File $TitlePath
        if ($Cfg.schema -ne 'pes-title-1' -or -not $Cfg.expect -or -not $Cfg.title_id -or -not $Cfg.save_title_id) {
            throw 'not a pes-title-1 config with title_id, save_title_id and expect'
        }
        $CfgOk = $true
        Add-Step 'title_config' 'PASS' ('schema pes-title-1, title_id ' + $Cfg.title_id)
    } catch {
        Add-Step 'title_config' 'FAIL' ('cannot load title config: ' + $_.Exception.Message)
    }

    # Build (C9 gate is separate, below).
    $BuildOk = $false
    if ($SkipBuild) {
        Add-Step 'build' 'SKIP' '-SkipBuild given; using the existing executable'
    } elseif (-not $HasMsys) {
        Add-Step 'build' 'FAIL' ('MSYS2 not found at ' + $MsysRoot + ' (set MSYS2_ROOT); build not run')
    } else {
        $rb = Run-Msys './build.sh' (Join-Path $V 'M2-build.log')
        if ($rb.Exit -eq 0 -and -not $rb.TimedOut) {
            $BuildOk = $true
            Add-Step 'build' 'PASS' 'build.sh exit 0' $rb.Seconds 0
        } else {
            Add-Step 'build' 'FAIL' ('build.sh exit ' + $rb.Exit + ', timed out ' + $rb.TimedOut + ': ' + (Last-Line ($rb.Stdout + $rb.Stderr))) $rb.Seconds $rb.Exit
        }
    }
    $ExeSha = ''
    $ExeExists = Test-Path -LiteralPath $Exe -PathType Leaf
    if ($ExeExists) { $ExeSha = Hash-File $Exe }

    # prepare.py, twice (C1).
    function Run-Prepare([string]$Dir, [string]$Name) {
        if (Test-Path -LiteralPath $Dir) { Remove-Item -LiteralPath $Dir -Recurse -Force }
        $extra = @()
        if ($AllowHashMismatch) { $extra = @('--allow-hash-mismatch') }
        $rp = Run-Py (Join-Path $Root 'scripts\prepare.py') (@($GameDir, '--title', $TitlePath, '--out', $Dir) + $extra) (Join-Path $V ('M2-' + $Name + '.log')) 1800000
        if ($rp.Exit -eq 0 -and -not $rp.TimedOut -and (Test-Path -LiteralPath (Join-Path $Dir 'boot.bin'))) {
            Add-Step $Name 'PASS' 'prepare.py exit 0' $rp.Seconds 0
            return $true
        }
        Add-Step $Name 'FAIL' ('prepare.py exit ' + $rp.Exit + ', timed out ' + $rp.TimedOut + ': ' + (Last-Line ($rp.Stderr + $rp.Stdout))) $rp.Seconds $rp.Exit
        return $false
    }
    $Prep1 = $false
    $Prep2 = $false
    if ($GameOk -and $CfgOk) {
        $Prep1 = Run-Prepare $Run1 'prepare-run1'
        $Prep2 = Run-Prepare $Run2 'prepare-run2'
    } else {
        Add-Step 'prepare-run1' 'FAIL' 'not run: game directory or title config unusable'
        Add-Step 'prepare-run2' 'FAIL' 'not run: game directory or title config unusable'
    }
    $BootSha1 = ''
    $BootSha2 = ''
    if ($Prep1 -and $Prep2) {
        $BootSha1 = Hash-File (Join-Path $Run1 'boot.bin')
        $BootSha2 = Hash-File (Join-Path $Run2 'boot.bin')
        $AnaSha1 = Hash-File (Join-Path $Run1 'analysis.json')
        $AnaSha2 = Hash-File (Join-Path $Run2 'analysis.json')
        $c1Values = [ordered]@{ boot_sha256_run1 = $BootSha1; boot_sha256_run2 = $BootSha2; analysis_sha256_run1 = $AnaSha1; analysis_sha256_run2 = $AnaSha2 }
        if ($BootSha1 -eq $BootSha2 -and $AnaSha1 -eq $AnaSha2) {
            Set-Crit 'C1' 'PASS' 'two prepare runs give identical boot.bin and analysis.json' $c1Values
        } else {
            Set-Crit 'C1' 'FAIL' 'the two prepare runs differ' $c1Values
        }
    } else {
        Set-Crit 'C1' 'FAIL' 'prepare.py did not complete twice; see the prepare steps' ([ordered]@{ run1_ok = $Prep1; run2_ok = $Prep2 })
    }

    # analysis.json from run 1 (the output used for the rest).
    $An = $null
    if ($Prep1) {
        try { $An = (Read-Utf8 (Join-Path $Run1 'analysis.json')) | ConvertFrom-Json }
        catch { Add-Step 'analysis' 'FAIL' ('cannot read analysis.json: ' + $_.Exception.Message) }
    }

    # Content profile (C8 input).
    $ContentOk = $false
    if ($Prep1) {
        $rc = Run-Py (Join-Path $Root 'scripts\content_profile.py') @($GameDir, '--out', $Run1) (Join-Path $V 'M2-content.log') 600000
        if ($rc.Exit -eq 0 -and -not $rc.TimedOut -and (Test-Path -LiteralPath (Join-Path $Run1 'content.bin'))) {
            $ContentOk = $true
            Add-Step 'content_profile' 'PASS' 'content_profile.py exit 0' $rc.Seconds 0
        } else {
            Add-Step 'content_profile' 'FAIL' ('content_profile.py exit ' + $rc.Exit + ': ' + (Last-Line ($rc.Stderr + $rc.Stdout))) $rc.Seconds $rc.Exit
        }
    } else {
        Add-Step 'content_profile' 'FAIL' 'not run: prepare run 1 did not complete'
    }

    # Loader: stop before entry, import report, image dump (C2, C3, C5, C6).
    foreach ($stale in @($TsvPath, $DumpPath, $LoaderLog)) {
        if (Test-Path -LiteralPath $stale) { Remove-Item -LiteralPath $stale -Force }
    }
    $LoaderR = $null
    $LoaderRan = $false
    $LoaderReason = ''
    if (-not $Prep1) { $LoaderReason = 'prepare run 1 did not complete' }
    elseif (-not $ContentOk) { $LoaderReason = 'content profile step failed' }
    elseif (-not $ExeExists) { $LoaderReason = 'loader not built: ' + $Exe + ' (run without -SkipBuild, or build first)' }
    if ($LoaderReason) {
        Add-Step 'loader' 'FAIL' ('not run: ' + $LoaderReason)
    } else {
        $LoaderR = Run-Native -Exe $Exe -ArgList @(
            (Join-Path $Run1 'boot.bin'), '--cpu-only', '--stop-before-entry', '--app0', $GameDir,
            '--import-aliases', (Join-Path $Run1 'import-aliases.txt'),
            '--import-report', $TsvPath, '--dump-image', $DumpPath,
            '--content-profile', (Join-Path $Run1 'content.bin')) -LogFile $LoaderLog -TimeoutMs 900000 -EnvVars $LoaderEnv
        if ($LoaderR.TimedOut) {
            $LoaderReason = 'loader timed out'
            Add-Step 'loader' 'FAIL' 'loader timed out' $LoaderR.Seconds
        } else {
            $LoaderRan = $true
            if ($LoaderR.Exit -eq 0) {
                Add-Step 'loader' 'PASS' 'loader exit 0' $LoaderR.Seconds 0
            } else {
                Add-Step 'loader' 'FAIL' ('loader exit ' + $LoaderR.Exit + ': ' + (Last-Line $LoaderR.Stderr)) $LoaderR.Seconds $LoaderR.Exit
            }
        }
    }
    $Combined = ''
    if ($LoaderR) { $Combined = $LoaderR.Stdout + $LoaderR.Stderr }

    # C2: entry and the Entering line.
    if ($LoaderRan -and $An -and $CfgOk) {
        $entryAn = Parse-Hex $An.entry
        $entryExp = Parse-Hex $Cfg.expect.entry
        $m = [regex]::Match($Combined, '(?m)^Entering original x86-64 code at guest offset 0x([0-9a-fA-F]+)\r?$')
        $entered = $null
        if ($m.Success) { $entered = Parse-Hex ('0x' + $m.Groups[1].Value) }
        $c2Values = [ordered]@{ entry_analysis = ('0x{0:x}' -f $entryAn); entry_expected = ('0x{0:x}' -f $entryExp); entering_line_offset = $(if ($null -eq $entered) { 'not printed' } else { ('0x{0:x}' -f $entered) }); loader_exit = $LoaderR.Exit }
        if ($entryAn -eq $entryExp -and $null -ne $entered -and $entered -eq $entryExp -and $LoaderR.Exit -eq 0) {
            Set-Crit 'C2' 'PASS' 'entry is the expected offset, the Entering line matches, exit 0' $c2Values
        } else {
            Set-Crit 'C2' 'FAIL' 'entry, Entering line or exit code differs from the title config' $c2Values
        }
    } else {
        Set-Crit 'C2' 'FAIL' ('not measured: ' + $LoaderReason) ([ordered]@{})
    }

    # C3: image dump hash (dump is deleted in the finally block).
    if ($LoaderRan -and $An -and (Test-Path -LiteralPath $DumpPath)) {
        $dumpSha = Hash-File $DumpPath
        $dumpSize = (Get-Item -LiteralPath $DumpPath).Length
        $c3Values = [ordered]@{ dump_bytes = $dumpSize; image_bytes = [int64]$An.image_bytes; dump_sha256 = $dumpSha; image_sha256 = $An.image_sha256 }
        if ($dumpSha -eq $An.image_sha256 -and $dumpSize -eq [int64]$An.image_bytes) {
            Set-Crit 'C3' 'PASS' 'SHA-256 of the pre-relocation image dump equals analysis.json image_sha256' $c3Values
        } else {
            Set-Crit 'C3' 'FAIL' 'image dump differs from analysis.json image_sha256 or image_bytes' $c3Values
        }
    } else {
        Set-Crit 'C3' 'FAIL' ('not measured: ' + $(if ($LoaderReason) { $LoaderReason } else { 'no image dump written' })) ([ordered]@{})
    }

    # C4: relocations (no loader needed; the loader's own count is recorded for comparison).
    if ($An -and $CfgOk) {
        $written = [int64]$An.relocations_written
        $expR = [int64]$Cfg.expect.relocations_written
        $total = [int64]$An.relocation_records_total
        $appliedMatch = [regex]::Match($Combined, 'applied (\d+) relocations')
        $applied = $null
        if ($appliedMatch.Success) { $applied = [int64]$appliedMatch.Groups[1].Value }
        $c4Values = [ordered]@{
            relocations_written = $written; relocations_expected = $expR; relocation_records_total = $total
            records_not_written = ($total - $written); relocations_skipped_unmapped = 0; loader_applied = $applied
        }
        if ($written -eq $expR) {
            Set-Crit 'C4' 'PASS' 'relocations_written equals the title config expectation; skipped_unmapped is 0 (prepare.py raises on any unmapped relocation)' $c4Values
        } else {
            Set-Crit 'C4' 'EXPLAIN' ('relocations_written ' + $written + ' differs from expected ' + $expR + ' (records total ' + $total + '); explain in knowledge.md before sign-off (D-M2-5)') $c4Values
        }
    } else {
        Set-Crit 'C4' 'FAIL' 'not measured: analysis.json or title config unusable' ([ordered]@{})
    }

    # C5: import report (TSV) against the Imports: summary line and the title config.
    $expImports = -1
    if ($CfgOk) { $expImports = [int64]$Cfg.expect.imports }
    if ($LoaderRan -and (Test-Path -LiteralPath $TsvPath)) {
        $tsvLines = [IO.File]::ReadAllLines($TsvPath, $Utf8)
        $header = "index`tname`tkind`tlooked_up_as`tresult`tsymbol`tprefix_candidate"
        $headerOk = ($tsvLines.Count -ge 1 -and $tsvLines[0] -eq $header)
        $rows = New-Object System.Collections.ArrayList
        $badRows = 0
        for ($i = 1; $i -lt $tsvLines.Count; $i++) {
            if ($tsvLines[$i] -eq '') { continue }
            $f = $tsvLines[$i].Split("`t")
            if ($f.Count -ne 7) { $badRows++; continue }
            [void]$rows.Add([pscustomobject]@{ name = $f[1]; result = $f[4]; candidate = $f[6] })
        }
        $tsvHost = @($rows | Where-Object { $_.result -eq 'host' }).Count
        $tsvNative = @($rows | Where-Object { $_.result -eq 'native' }).Count
        $tsvTrap = @($rows | Where-Object { $_.result -eq 'trap' }).Count
        $offendingRows = @($rows | Where-Object { $_.result -eq 'trap' -and $_.candidate -ne '-' })
        $tsvPrefix = $offendingRows.Count
        $offending = @($offendingRows | Select-Object -First 20 | ForEach-Object { $_.name })
        $sumMatch = [regex]::Match($Combined, '(?m)^Imports: (\d+) host, (\d+) native, (\d+) trap, (\d+) prefix_only\r?$')
        $sumCount = [regex]::Matches($Combined, '(?m)^Imports: ').Count
        $srcHits = @(Get-ChildItem -LiteralPath (Join-Path $Root 'src') -Recurse -Include *.c, *.h -File | Select-String -Pattern 'strncmp\([^)]*,\s*11\)')
        $strncmpHits = $srcHits.Count
        $n = $rows.Count
        $c5Values = [ordered]@{
            summary_host = $null; summary_native = $null; summary_trap = $null; summary_prefix_only = $null
            tsv_rows = $n; tsv_host = $tsvHost; tsv_native = $tsvNative; tsv_trap = $tsvTrap; tsv_prefix_only = $tsvPrefix
            imports_expected = $expImports; strncmp_11_in_src = $strncmpHits; offending_first_20 = $offending
        }
        if ($sumMatch.Success) {
            $c5Values['summary_host'] = [int64]$sumMatch.Groups[1].Value
            $c5Values['summary_native'] = [int64]$sumMatch.Groups[2].Value
            $c5Values['summary_trap'] = [int64]$sumMatch.Groups[3].Value
            $c5Values['summary_prefix_only'] = [int64]$sumMatch.Groups[4].Value
        }
        $consistent = $sumMatch.Success -and $sumCount -eq 1 -and $headerOk -and $badRows -eq 0 `
            -and $c5Values['summary_host'] -eq $tsvHost -and $c5Values['summary_native'] -eq $tsvNative `
            -and $c5Values['summary_trap'] -eq $tsvTrap -and $c5Values['summary_prefix_only'] -eq $tsvPrefix
        if (-not $consistent) {
            Set-Crit 'C5' 'FAIL' 'import report is malformed or does not match the Imports: summary line' $c5Values
        } elseif ($n -ne $expImports) {
            Set-Crit 'C5' 'FAIL' ('import report has ' + $n + ' rows, title config expects ' + $expImports) $c5Values
        } elseif ($tsvPrefix -ne 0) {
            Set-Crit 'C5' 'FAIL' ('prefix_only = ' + $tsvPrefix + ' (expected 0): imports that only the old 11-character rule would resolve; see offending_first_20 and the suffix map (T7)') $c5Values
        } elseif ($strncmpHits -ne 0) {
            Set-Crit 'C5' 'FAIL' ('strncmp(..., 11) still in src: ' + $strncmpHits + ' match(es)') $c5Values
        } else {
            Set-Crit 'C5' 'PASS' 'all imports resolve exactly; summary and TSV agree; prefix_only 0' $c5Values
        }
    } else {
        Set-Crit 'C5' 'FAIL' ('not measured: ' + $(if ($LoaderReason) { $LoaderReason } else { 'import report not written' })) ([ordered]@{ imports_expected = $expImports })
    }

    # C6: CPU check (loader message "this CPU lacks ...", from check_cpu in src/main.c).
    if ($LoaderRan) {
        $cpuMatch = [regex]::Match($Combined, '(?m)^ERROR: this CPU lacks.*$')
        $c6Values = [ordered]@{ cpu = $CpuName; loader_exit = $LoaderR.Exit; loader_output_chars = $Combined.Length }
        if ($cpuMatch.Success) {
            Set-Crit 'C6' 'FAIL' ('check_cpu reported missing CPU features: ' + $cpuMatch.Value.Trim()) $c6Values
        } elseif ($LoaderR.Exit -eq 0) {
            Set-Crit 'C6' 'PASS' 'check_cpu passed (exit 0 and no "this CPU lacks" message in the loader log)' $c6Values
        } else {
            Set-Crit 'C6' 'FAIL' ('no CPU-check result: loader exit ' + $LoaderR.Exit + ' with no "this CPU lacks" message (loader output ' + $Combined.Length + ' chars); the loader stopped before or during check_cpu') $c6Values
        }
    } else {
        Set-Crit 'C6' 'FAIL' ('not measured: ' + $(if ($LoaderReason) { $LoaderReason } else { 'loader did not run' })) ([ordered]@{ cpu = $CpuName })
    }

    # C7a: GNM table (region bytes from the image dump; spec-t5 section 3). C7b: N/A on Windows.
    $c7aValues = [ordered]@{ objdump = $ObjdumpVersion; region_lo = $null; region_hi = $null; objdump_exit = $null; callgraph_exit = $null; sigs_exit = $null; generator_exit = $null; generator_output = $null; table_rows = $null; table_sha256 = $null }
    if (-not $CfgOk) {
        Set-Crit 'C7a' 'FAIL' 'title config unusable' $c7aValues
    } elseif ($null -eq $Cfg.gnm_region) {
        Set-Crit 'C7a' 'FAIL' 'gnm_region is null in the title config (region not pinned)' $c7aValues
    } elseif (-not $ObjdumpExe) {
        Set-Crit 'C7a' 'SKIP' ('GNU objdump not available: ' + $ObjdumpNote + '; GNM table not generated') $c7aValues
    } elseif (-not (Test-Path -LiteralPath $DumpPath)) {
        Set-Crit 'C7a' 'FAIL' ('not measured: ' + $(if ($LoaderReason) { $LoaderReason } else { 'no image dump' })) $c7aValues
    } else {
        $lo = [int64]$Cfg.gnm_region.lo
        $hi = [int64]$Cfg.gnm_region.hi
        $dumpLen = (Get-Item -LiteralPath $DumpPath).Length
        $loHex = '0x{0:x}' -f $lo
        $hiHex = '0x{0:x}' -f $hi
        $c7aValues['region_lo'] = $loHex
        $c7aValues['region_hi'] = $hiHex
        if ($lo -lt 0 -or $hi -le $lo -or $hi -gt $dumpLen) {
            Set-Crit 'C7a' 'FAIL' 'gnm_region is outside the image dump' $c7aValues
        } else {
            New-Item -ItemType Directory -Force -Path $GnmDir | Out-Null
            $regionBin = Join-Path $GnmDir 'region.bin'
            $regionAsm = Join-Path $GnmDir 'region.asm'
            $cgPath = Join-Path $GnmDir 'gnm_cg.txt'
            $sigPath = Join-Path $GnmDir 'gnm_sigs.txt'
            $len = [int]($hi - $lo)
            $buf = New-Object byte[] $len
            $fs = [IO.File]::OpenRead($DumpPath)
            try {
                $fs.Position = $lo
                $got = 0
                while ($got -lt $len) {
                    $k = $fs.Read($buf, $got, $len - $got)
                    if ($k -le 0) { break }
                    $got += $k
                }
            } finally { $fs.Close() }
            [IO.File]::WriteAllBytes($regionBin, $buf)
            $rOd = Run-Native -Exe $ObjdumpExe -ArgList @('-D', '-b', 'binary', '-m', 'i386:x86-64', '-M', 'intel', '--insn-width=16', ('--adjust-vma=' + $loHex), $regionBin) -StdoutFile $regionAsm -LogFile (Join-Path $GnmDir 'objdump.err.log')
            Remove-Item -LiteralPath $regionBin -Force
            $rCg = Run-Py (Join-Path $Root 'scripts\gnm\callgraph.py') @((Join-Path $Run1 'eboot.elf'), $loHex, $hiHex, $cgPath) (Join-Path $GnmDir 'callgraph.log')
            $rSig = Run-Py (Join-Path $Root 'scripts\gnm\gnm_sigs.py') @($regionAsm, $cgPath) (Join-Path $GnmDir 'gnm_sigs.err.log') 900000 $sigPath
            $regionArg = $loHex + '-' + $hiHex
            if (Test-Path -LiteralPath $GenOut) { Remove-Item -LiteralPath $GenOut -Force }
            $rGen = Run-Py (Join-Path $Root 'scripts\gen_gnm_table.py') @('--region', $regionArg, $regionAsm, $cgPath, $sigPath, '-o', $GenOut) (Join-Path $GnmDir 'gen_gnm_table.log')
            $genLine = Last-Line $rGen.Stdout
            $rows = 0
            $tableSha = ''
            if (Test-Path -LiteralPath $GenOut) {
                $rows = @([IO.File]::ReadAllLines($GenOut) | Where-Object { $_ -match '^\{0x' }).Count
                $tableSha = Hash-File $GenOut
            }
            $c7aValues['objdump_exit'] = $rOd.Exit
            $c7aValues['callgraph_exit'] = $rCg.Exit
            $c7aValues['sigs_exit'] = $rSig.Exit
            $c7aValues['generator_exit'] = $rGen.Exit
            $c7aValues['generator_output'] = $genLine
            $c7aValues['table_rows'] = $rows
            $c7aValues['table_sha256'] = $tableSha
            $steps7 = @($rOd.Exit, $rCg.Exit, $rSig.Exit, $rGen.Exit)
            if ($rOd.Exit -eq 0 -and $rCg.Exit -eq 0 -and $rSig.Exit -eq 0 -and $rGen.Exit -eq 0 -and $rows -gt 0) {
                Set-Crit 'C7a' 'PASS' ('generator exit 0; ' + $rows + ' rows in src\title\generated\pes_gnm_table.inc; gnm_region non-null') $c7aValues
            } else {
                Set-Crit 'C7a' 'FAIL' ('GNM steps exit codes objdump/callgraph/sigs/generator = ' + ($steps7 -join '/') + ', rows ' + $rows + '; see out\verify\gnm') $c7aValues
            }
        }
    }
    Set-Crit 'C7b' 'NA' 'N/A on the Windows build: gpu/CMakeLists.txt excludes pes_gnm_hooks.cpp under WIN32, so there is no GNM binding census here (moves to M4, U-5). PES_GNM_CENSUS is not used.' ([ordered]@{})

    # C8: ids, then the save root the loader logs. The loader prints "User directory: <path>" at startup and
    # "Save root: <user>/savedata/1/<title>" when savedata is configured (--app0 given; src/main.c, src/runtime_savedata.c).
    if (-not $ContentOk -or -not $CfgOk) {
        Set-Crit 'C8' 'FAIL' 'not measured: content profile or title config unusable' ([ordered]@{})
    } else {
        $cp = (Read-Utf8 (Join-Path $Run1 'content-profile.json')) | ConvertFrom-Json
        $sfoSave = $null
        if ($An -and $An.sfo -and ($An.sfo.PSObject.Properties.Name -contains 'INSTALL_DIR_SAVEDATA')) { $sfoSave = $An.sfo.INSTALL_DIR_SAVEDATA }
        $udMatch = [regex]::Match($Combined, '(?m)^User directory: (.+?)\r?$')
        $srMatch = [regex]::Match($Combined, '(?m)^Save root: (.+?)\r?$')
        $udLine = $(if ($udMatch.Success) { $udMatch.Groups[1].Value } else { $null })
        $srLine = $(if ($srMatch.Success) { $srMatch.Groups[1].Value } else { $null })
        $RepoUser = Join-Path $Root 'user'
        $c8Values = [ordered]@{
            content_title_id = $cp.title_id; config_title_id = $Cfg.title_id; config_save_title_id = $Cfg.save_title_id
            param_sfo_INSTALL_DIR_SAVEDATA = $sfoSave; user_directory_reported = $udLine; save_root_reported = $srLine
            repo_user_directory = $RepoUser; game_directory = $GameDir
        }
        if ($cp.title_id -ne $Cfg.save_title_id -or $Cfg.title_id -ne $Cfg.save_title_id) {
            Set-Crit 'C8' 'FAIL' 'content profile title_id, config title_id and save_title_id do not agree' $c8Values
        } elseif ($Cfg.save_title_id -ne 'CUSA04957') {
            Set-Crit 'C8' 'FAIL' 'title id is not CUSA04957' $c8Values
        } elseif ($sfoSave -and $sfoSave -ne $Cfg.save_title_id) {
            Set-Crit 'C8' 'FAIL' 'param.sfo INSTALL_DIR_SAVEDATA differs from the title id; the loader uses it for the save directory (src/main.c, savedata configure)' $c8Values
        } elseif (-not $LoaderRan) {
            Set-Crit 'C8' 'EXPLAIN' 'ids match CUSA04957; the loader did not run, so the save root is not measured' $c8Values
        } elseif (-not $udLine -or -not $srLine) {
            Set-Crit 'C8' 'EXPLAIN' 'ids match CUSA04957; the loader log has no "User directory:" or "Save root:" line, so the save root is not measured (the loader prints Save root only with --app0 and a title id)' $c8Values
        } else {
            $udN = Norm-Path $udLine
            $srN = Norm-Path $srLine
            $repoN = Norm-Path $RepoUser
            $gameN = Norm-Path $GameDir
            $expectN = Norm-Path ($udLine.TrimEnd('/', '\') + '\savedata\1\CUSA04957')
            $c8Values['user_directory_normalized'] = $udN
            $c8Values['save_root_normalized'] = $srN
            if (-not $srN -or -not $repoN -or -not $gameN -or -not $udN -or -not $expectN) {
                Set-Crit 'C8' 'EXPLAIN' 'a path from the loader log could not be normalized; the save root is not measured' $c8Values
            } elseif ($srN -eq $gameN -or $srN.StartsWith($gameN + '\')) {
                Set-Crit 'C8' 'FAIL' ('save root ' + $srLine + ' is inside the game directory ' + $GameDir + '; saves must go to the repo user directory') $c8Values
            } elseif ($srN -ne $expectN) {
                Set-Crit 'C8' 'FAIL' ('save root ' + $srLine + ' is not <user>\savedata\1\CUSA04957 for the reported user directory ' + $udLine) $c8Values
            } elseif ($srN.StartsWith($repoN + '\')) {
                Set-Crit 'C8' 'PASS' ('ids match CUSA04957; the loader save root ' + $srLine + ' is under the repo user directory and not under the game directory') $c8Values
            } else {
                Set-Crit 'C8' 'EXPLAIN' ('ids match CUSA04957; the loader user directory ' + $udLine + ' is not the repo user directory ' + $RepoUser + ' (PES_USER_DIR or --user set?), so the save root is not checked against the repo') $c8Values
            }
        }
    }

    # C9: agent-side gate in the MSYS2 CLANG64 shell.
    if (-not $HasMsys) {
        Set-Crit 'C9' 'SKIP' ('MSYS2 not found at ' + $MsysRoot + ': agent-side gate not run') ([ordered]@{})
    } else {
        $gates = @(
            @{ name = 'build-test'; cmd = './build.sh --test' },
            @{ name = 'py-compile'; cmd = 'python3 -m py_compile $(git ls-files ''*.py'')' },
            @{ name = 'unittest'; cmd = 'python3 -m unittest discover -s tests' },
            @{ name = 'audit-publish'; cmd = 'python3 scripts/audit_publish.py' }
        )
        $gateExit = @{}
        $auditWarn = -1
        $auditPass = $false
        foreach ($g in $gates) {
            $rg = Run-Msys $g.cmd (Join-Path $V ('M2-gate-' + $g.name + '.log')) 3600000
            $gateExit[$g.name] = $rg.Exit
            if ($rg.Exit -eq 0 -and -not $rg.TimedOut) {
                Add-Step ('gate-' + $g.name) 'PASS' ($g.cmd + ' exit 0') $rg.Seconds 0
            } else {
                Add-Step ('gate-' + $g.name) 'FAIL' ($g.cmd + ' exit ' + $rg.Exit + ', timed out ' + $rg.TimedOut + ': ' + (Last-Line ($rg.Stdout + $rg.Stderr))) $rg.Seconds $rg.Exit
            }
            if ($g.name -eq 'audit-publish') {
                $auditText = $rg.Stdout + $rg.Stderr
                $auditWarn = ([regex]::Matches($auditText, '(?m)^WARN\b')).Count
                $auditPass = ([regex]::Matches($auditText, '(?m)^PASS:')).Count -gt 0
            }
        }
        $c9Values = [ordered]@{
            build_test_exit = $gateExit['build-test']; py_compile_exit = $gateExit['py-compile']
            unittest_exit = $gateExit['unittest']; audit_publish_exit = $gateExit['audit-publish']
            audit_warn_lines = $auditWarn; audit_pass_line = $auditPass
        }
        if ($gateExit['build-test'] -eq 0 -and $gateExit['py-compile'] -eq 0 -and $gateExit['unittest'] -eq 0 `
            -and $gateExit['audit-publish'] -eq 0 -and $auditWarn -eq 0 -and $auditPass) {
            Set-Crit 'C9' 'PASS' 'build.sh --test, py_compile, unittest and audit_publish (PASS, 0 WARN) all exit 0' $c9Values
        } else {
            Set-Crit 'C9' 'FAIL' 'an agent-side gate item failed; see the gate steps and logs' $c9Values
        }
    }
}
catch {
    Add-Step 'internal' 'FAIL' ('script error: ' + $_.Exception.Message)
    $ExitCode = 1
}
finally {
    try {
        $Summary = [ordered]@{ PASS = 0; FAIL = 0; EXPLAIN = 0; SKIP = 0; NA = 0 }
        foreach ($id in $CritIds) {
            $st = $Crit[$id].status
            $Summary[$st] = [int]$Summary[$st] + 1
        }
        # Precedence: FAIL > INCOMPLETE > EXPLAIN > PASS. Any SKIP (NA is not SKIP) makes it INCOMPLETE.
        $overall = 'PASS'
        if ($Summary.EXPLAIN -gt 0) { $overall = 'EXPLAIN' }
        if ($Summary.SKIP -gt 0) { $overall = 'INCOMPLETE' }
        if ($Summary.FAIL -gt 0) { $overall = 'FAIL' }
        $finished = Get-Date
        $Result = [ordered]@{
            schema = 'pes-verify-m2-1'
            overall = $overall
            summary = $Summary
            started = $StartedAt.ToString('o')
            finished = $finished.ToString('o')
            elapsed_seconds = [math]::Round(($finished - $StartedAt).TotalSeconds, 1)
            game_dir = $GameDir
            title_config = [ordered]@{ path = $TitleConfig; sha256 = $CfgSha; title_id = $(if ($CfgOk) { $Cfg.title_id } else { $null }) }
            input = [ordered]@{ file = $InputName; md5 = $InputMd5; sha256 = $InputSha; param_sfo_sha256 = $SfoSha }
            exe = [ordered]@{ built_this_run = $BuildOk; sha256 = $ExeSha }
            boot_bin_sha256 = $BootSha1
            cpu = $CpuName
            os = $OsName
            python = $PyVersion
            objdump = $ObjdumpVersion
            msys2_present = $HasMsys
            criteria = @($CritIds | ForEach-Object { $Crit[$_] })
            steps = @($Steps.ToArray())
        }
        try {
            if (Test-Path -LiteralPath $DumpPath) { Remove-Item -LiteralPath $DumpPath -Force }
            $regionLeft = Join-Path $GnmDir 'region.bin'
            if (Test-Path -LiteralPath $regionLeft) { Remove-Item -LiteralPath $regionLeft -Force }
        } catch { Log ('cleanup failed: ' + $_.Exception.Message) }
        $json = ($Result | ConvertTo-Json -Depth 12).Replace("`r`n", "`n")
        Save-Text $ResultPath ($json + "`n")
        Log ('result: ' + $ResultPath + ' overall ' + $overall)
        foreach ($id in $CritIds) {
            Write-Host ('{0,-4} {1,-8} {2}' -f $id, $Crit[$id].status, $Crit[$id].detail)
        }
        if ($overall -eq 'FAIL') { $ExitCode = 1 }
    } catch {
        Write-Host ('could not write result JSON: ' + $_.Exception.Message)
        $ExitCode = 1
    }
}
exit $ExitCode
