# Windows Robot Framework launchers

Run BAT files from PowerShell with the current-directory prefix, for example:

```powershell
.\run_robot_read_only.bat G3
```

| Launcher | Purpose |
|---|---|
| `setup_robot_environment.bat` | Create/repair `.venv` and install Robot/HIL dependencies |
| `run_robot_read_only.bat` | Protocol, UI, diagnostics, memory, performance, and coherent snapshot |
| `run_robot_safe_output.bat` | Read-only coverage plus zero-mask, repeated all-off, and G3 transport stress |
| `run_robot_hil_single_channel.bat` | Full physical verification on the selected DMM-connected channel |
| `run_robot_gate3_transport_fault.bat` | Special test-image proof that queued-invalidated and timed-out commands never actuate |
| `run_robot_all_safe.bat` | Read-only followed by safe-output |
| `run_robot_custom.bat` | Direct access to supported Robot runner options |
| `validate_robot_suites.bat` | Static suite/method mapping validation |

Copy `robot_framework\variables\bench_config.example.bat` to `bench_config.local.bat`. The local file is loaded automatically and is not intended to be replaced during package upgrades.
