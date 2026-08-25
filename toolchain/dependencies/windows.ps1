# Copyright (c) - Graphical Playground. All rights reserved.
# For more information, see https://graphical-playground/legal
# mailto:support AT graphical-playground DOT com

#requires -Version 5.1

$ErrorActionPreference = "Stop"

# ------------------------------------------------------------
# Self-elevate to Administrator
# ------------------------------------------------------------

$principal = New-Object Security.Principal.WindowsPrincipal(
  [Security.Principal.WindowsIdentity]::GetCurrent()
)

$isAdmin = $principal.IsInRole(
  [Security.Principal.WindowsBuiltInRole]::Administrator
)

if (-not $isAdmin) {
  Write-Host "Not running as Administrator. Requesting elevation..." -ForegroundColor Yellow

  Start-Process powershell.exe `
    -Verb RunAs `
    -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""

  exit
}

Write-Host "Running as Administrator." -ForegroundColor Green

# ------------------------------------------------------------
# Check / install Chocolatey
# ------------------------------------------------------------

if (-not (Get-Command choco.exe -ErrorAction SilentlyContinue)) {
  Write-Host "Chocolatey is not installed. Installing..." -ForegroundColor Yellow

  Set-ExecutionPolicy Bypass -Scope Process -Force

  [System.Net.ServicePointManager]::SecurityProtocol =
    [System.Net.ServicePointManager]::SecurityProtocol -bor 3072

  Invoke-Expression (
    (New-Object System.Net.WebClient).DownloadString(
      'https://community.chocolatey.org/install.ps1'
    )
  )

  # Refresh PATH so choco is available in this process
  $env:Path = [System.Environment]::GetEnvironmentVariable(
    "Path",
    "Machine"
  ) + ";" + [System.Environment]::GetEnvironmentVariable(
    "Path",
    "User"
  )
}

if (-not (Get-Command choco.exe -ErrorAction SilentlyContinue)) {
  throw "Chocolatey installation failed or choco.exe could not be found."
}

Write-Host "Chocolatey found:" -ForegroundColor Green
choco --version

# ------------------------------------------------------------
# Install development tools
# ------------------------------------------------------------

Write-Host ""
Write-Host "Installing/updating development tools..." -ForegroundColor Cyan

choco install llvm cmake git ninja -y --no-progress

if ($LASTEXITCODE -ne 0) {
  Write-Warning "One or more Chocolatey packages may have failed."
}
else {
  Write-Host "LLVM / CMake / Git / Ninja installation complete." -ForegroundColor Green
}

# ------------------------------------------------------------
# Done
# ------------------------------------------------------------

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Development environment check complete "
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

Read-Host "Press Enter to exit"
