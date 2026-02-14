# Copy WiiMedic to USB drive E:\apps\WiiMedic
# Run from the WiiMedic project folder. Copies meta.xml (and boot.dol/icon.png if present).

$ProjectRoot = $PSScriptRoot
$UsbAppPath = "E:\apps\WiiMedic"

if (-not (Test-Path $UsbAppPath)) {
    New-Item -ItemType Directory -Path $UsbAppPath -Force | Out-Null
}

$copied = @()
if (Test-Path (Join-Path $ProjectRoot "boot.dol")) {
    Copy-Item -Path (Join-Path $ProjectRoot "boot.dol") -Destination (Join-Path $UsbAppPath "boot.dol") -Force
    $copied += "boot.dol"
}
if (Test-Path (Join-Path $ProjectRoot "meta.xml")) {
    Copy-Item -Path (Join-Path $ProjectRoot "meta.xml") -Destination (Join-Path $UsbAppPath "meta.xml") -Force
    $copied += "meta.xml"
}
if (Test-Path (Join-Path $ProjectRoot "icon.png")) {
    Copy-Item -Path (Join-Path $ProjectRoot "icon.png") -Destination (Join-Path $UsbAppPath "icon.png") -Force
    $copied += "icon.png"
}

if ($copied.Count -gt 0) {
    Write-Host "Copied to E:\apps\WiiMedic : $($copied -join ', ')" -ForegroundColor Green
} else {
    Write-Host "Nothing to copy. Add meta.xml (or build boot.dol) and run again." -ForegroundColor Yellow
}
