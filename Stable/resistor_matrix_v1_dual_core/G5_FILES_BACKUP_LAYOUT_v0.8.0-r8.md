# G5 Files / Backup Layout Update — v0.8.0-r8

## Purpose

Move all visible Backup tab actions into the Files tab so storage, file inventory, backup export, and factory-reset maintenance are grouped in one operator workflow.

## Changes

- Removed the visible **Backup** tab from the navigation bar.
- Added **Backup export** to the **Files** tab.
- Added **Factory reset** actions to the **Files** tab.
- Kept `/backup` as a compatibility page that links to `/files` and `/backup_download`.
- Kept `/backup_download` unchanged.
- Factory reset POST actions now return to `/files` after completion.
- Updated static asset cache-bust revision to `ui=filebackup-r8`.

## Validation status

Offline structural checks passed in this package. Arduino-Pico target compilation and connected hardware regression remain required.
