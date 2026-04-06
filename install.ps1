# human install script for Windows
# Run: irm https://h-uman.ai/install.ps1 | iex
# Installs human via WSL (Windows Subsystem for Linux)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "human — install for Windows" -ForegroundColor Green
Write-Host ""

# Check if WSL is available
$wslAvailable = $false
try {
    $wslVersion = wsl --version 2>$null
    if ($LASTEXITCODE -eq 0) { $wslAvailable = $true }
} catch {}

if (-not $wslAvailable) {
    try {
        $wslList = wsl --list --quiet 2>$null
        if ($LASTEXITCODE -eq 0 -and $wslList) { $wslAvailable = $true }
    } catch {}
}

if ($wslAvailable) {
    Write-Host "WSL detected. Installing human inside WSL..." -ForegroundColor Cyan
    wsl bash -c "curl -fsSL https://h-uman.ai/install.sh | sh"
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "human installed in WSL." -ForegroundColor Green
        Write-Host "Run: wsl human agent" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Tip: Add an alias to your PowerShell profile:" -ForegroundColor Gray
        Write-Host '  Set-Alias human { wsl human @args }' -ForegroundColor Gray
    } else {
        Write-Host "WSL install failed. Try manually:" -ForegroundColor Red
        Write-Host "  wsl bash -c 'curl -fsSL https://h-uman.ai/install.sh | sh'" -ForegroundColor Yellow
    }
} else {
    Write-Host "WSL is not installed." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "human requires Linux or macOS. On Windows, use WSL:" -ForegroundColor White
    Write-Host ""
    Write-Host "  1. Install WSL:" -ForegroundColor Cyan
    Write-Host "     wsl --install" -ForegroundColor White
    Write-Host ""
    Write-Host "  2. Restart your computer" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  3. Run this script again:" -ForegroundColor Cyan
    Write-Host "     irm https://h-uman.ai/install.ps1 | iex" -ForegroundColor White
    Write-Host ""
    Write-Host "  Or use Docker:" -ForegroundColor Cyan
    Write-Host "     docker pull ghcr.io/sethdford/h-uman:latest" -ForegroundColor White
    Write-Host "     docker run -it ghcr.io/sethdford/h-uman:latest agent" -ForegroundColor White
}
