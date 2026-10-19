#Requires -Version 5.1
<#
.SYNOPSIS
  Clone (optional), verify env ws7, then build+upload Kids Jobs firmware on Windows.

.EXAMPLE
  # From anywhere — wipe C:\js, clone fix branch, flash:
  powershell -ExecutionPolicy Bypass -File tools\windows_flash.ps1 -Bootstrap

.EXAMPLE
  # Already in C:\js on the right branch:
  powershell -ExecutionPolicy Bypass -File tools\windows_flash.ps1 -Upload
#>
param(
  [switch]$Bootstrap,
  [switch]$Upload,
  [switch]$BuildOnly,
  [string]$RepoUrl = "https://github.com/chrisomaney/jobsScreen.git",
  [string]$Branch = "cursor/fix-ws7-env-upload-43dc",
  [string]$ProjectDir = "C:\js"
)

$ErrorActionPreference = "Stop"

function Find-Pio {
  $candidates = @(
    (Get-Command pio -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
    "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe",
    "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
  ) | Where-Object { $_ -and (Test-Path $_) }
  if (-not $candidates) {
    throw @"
pio.exe not found.
Install PlatformIO Core, or open 'PlatformIO Core CLI' from VS Code/Cursor, then re-run.
Expected: $env:USERPROFILE\.platformio\penv\Scripts\pio.exe
"@
  }
  return $candidates[0]
}

function Assert-Ws7Project {
  param([string]$Dir)

  $ini = Join-Path $Dir "platformio.ini"
  if (-not (Test-Path $ini)) {
    throw "No platformio.ini in $Dir — wrong folder (do NOT use Downloads\jobsScreen-...)."
  }

  $text = Get-Content -Raw -Path $ini
  if ($text -notmatch '(?m)^\[env:ws7\]') {
    throw @"
WRONG TREE: $ini has no [env:ws7].

You are still on old main / old unzip. That is why you see:
  ProjectEnvsNotAvailableError: Please setup environments in platformio.ini file

Fix: re-run with -Bootstrap (clones branch $Branch into $ProjectDir).
"@
  }
  if ($text -notmatch '(?m)^default_envs\s*=\s*ws7\s*$') {
    Write-Warning "default_envs is not ws7 — continuing but prefer default_envs = ws7"
  }

  # Long path under Downloads is the classic MAX_PATH failure mode
  if ($Dir -match 'Downloads\\jobsScreen-' -or $Dir.Length -gt 40) {
    Write-Warning "Project path is long ($($Dir.Length) chars). Prefer C:\js to avoid Windows MAX_PATH / LVGL failures."
  }

  Write-Host "OK: [env:ws7] found in $ini" -ForegroundColor Green
  Select-String -Path $ini -Pattern "default_envs|\[env:ws7\]" | ForEach-Object { Write-Host "  $_" }
}

function Ensure-Secrets {
  param([string]$Dir)
  $secrets = Join-Path $Dir "include\secrets.h"
  $example = Join-Path $Dir "include\secrets.example.h"
  if (-not (Test-Path $secrets)) {
    if (-not (Test-Path $example)) { throw "Missing $example" }
    Copy-Item $example $secrets
    Write-Host "Created include\secrets.h from example — edit Wi-Fi/MQTT before relying on network." -ForegroundColor Yellow
  }
}

if ($Bootstrap) {
  if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "git not found on PATH. Install Git for Windows first."
  }
  Write-Host "=== Bootstrap: clone $Branch -> $ProjectDir ===" -ForegroundColor Cyan
  if (Test-Path $ProjectDir) {
    Write-Host "Removing $ProjectDir ..."
    Remove-Item -Recurse -Force $ProjectDir
  }
  git clone -b $Branch $RepoUrl $ProjectDir
  if ($LASTEXITCODE -ne 0) { throw "git clone failed (exit $LASTEXITCODE)" }
  $Upload = $true
}

if (-not (Test-Path $ProjectDir)) {
  throw "ProjectDir $ProjectDir does not exist. Pass -Bootstrap to clone it."
}

Set-Location $ProjectDir
Assert-Ws7Project -Dir $ProjectDir
Ensure-Secrets -Dir $ProjectDir

$pio = Find-Pio
Write-Host "Using pio: $pio" -ForegroundColor Cyan

if (Test-Path ".pio") {
  Write-Host "Cleaning .pio ..."
  Remove-Item -Recurse -Force .pio
}

if ($BuildOnly) {
  & $pio run -e ws7
  if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }
  Write-Host "BUILD OK" -ForegroundColor Green
  exit 0
}

if (-not $Upload -and -not $Bootstrap) {
  # Default when script is run with no switches from an existing tree: verify + upload
  $Upload = $true
}

if ($Upload) {
  Write-Host "=== Build + upload (env ws7) ===" -ForegroundColor Cyan
  Write-Host "Put the board in download mode if needed (BOOT held + RESET), USB plugged in."
  & $pio run -e ws7 -t upload
  if ($LASTEXITCODE -ne 0) { throw "Upload failed (exit $LASTEXITCODE)" }
  Write-Host "UPLOAD OK — starting serial monitor (Ctrl+C to stop)" -ForegroundColor Green
  & $pio device monitor -e ws7 -b 115200
}
