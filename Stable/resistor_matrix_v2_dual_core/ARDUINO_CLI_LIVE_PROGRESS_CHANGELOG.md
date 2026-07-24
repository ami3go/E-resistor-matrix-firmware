# Arduino CLI build runner 1.1.0

## Changes

- Added four visible build phases.
- Added a configurable elapsed-time heartbeat during quiet compiler periods.
- Added live incremental console and log output instead of buffering the complete build.
- Added `build_firmware_verbose.bat` for Arduino CLI `--verbose` compiler output.
- Added `build_and_flash_COM17_verbose.bat`.
- Added heartbeat output during upload.
- Preserved the native Arduino CLI exit code and avoided PowerShell `NativeCommandError` termination.
- Added build-runner version, verbose state and heartbeat interval to `build_manifest.json`.

## Commands

```bat
build_firmware.bat
build_firmware_verbose.bat
build_firmware.bat -HeartbeatSeconds 5
build_and_flash_COM17_verbose.bat
```

Standard mode prints a heartbeat every three seconds if no compiler output is received. Verbose mode additionally passes `--verbose` to Arduino CLI, which prints the complete external compiler command sequence.
