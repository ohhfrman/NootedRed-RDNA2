# Read-only Windows GPU identification for Ryzen 3 7320U / Radeon 610M.
param([string]$OutFile = "mendocino-hardware-report.txt")
$controllers = @(Get-CimInstance Win32_VideoController | Select-Object Name, PNPDeviceID, DriverVersion, VideoProcessor)
$devices = @(Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -match 'VEN_1002&DEV_1506' } |
    Select-Object Name, Manufacturer, PNPDeviceID, Status)
@(
  'NootedRed Mendocino R2 Windows read-only inventory'
  'All video controllers:'
  ($controllers | Format-List | Out-String)
  'Matching AMD Radeon 610M (VEN_1002&DEV_1506):'
  ($devices | Format-List | Out-String)
) | Out-File -LiteralPath $OutFile -Encoding utf8
Write-Host "Saved to $OutFile"
