# Calibration recovery

Do not reuse calibration from another E-Resistor board. The uploaded G3 report identified device serial `503359277A981F9F`, while the older accepted G1 archive identified `E66178758B3E742A`; those tables are not interchangeable.

Recover a bundle from a passing regression archive for the same board:

```powershell
.\extract_calibration_from_report.ps1 -ReportZip "C:\path\to\G2-passing-report.zip"
```

Then restore it. `restore_calibration.ps1` compares the bundle serial with the connected device and blocks a mismatch by default.

```powershell
.\restore_calibration.ps1 -FilePath ".\calibration-recovered-YYYYMMDD-HHMMSS.txt"
```

When no same-board backup exists, recalibrate the board rather than importing foreign tables.
