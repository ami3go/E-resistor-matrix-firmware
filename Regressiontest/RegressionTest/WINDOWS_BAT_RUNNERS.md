# Windows BAT runners

## Gate 5 acceptance

```powershell
.\setup_robot_environment.bat
.\run_robot_read_only.bat G5
.\run_robot_safe_output.bat G5
.\run_robot_gate5_service.bat G5
.\run_robot_hil_single_channel.bat G5
```

The service runner automatically loads the accepted Gate 4 baseline and executes 1,000 page requests with the five-percent `/state` p95 limit.

## HIL authorization

Set the following in `robot_framework\variables\bench_config.local.bat` only after the DMM is connected to one selected channel and all other channels are isolated:

```bat
set "ERESISTOR_ALLOW_ACTIVE_OUTPUT_TESTS=true"
set "ERESISTOR_FIXTURE_CONFIRMATION=E_RESISTOR_SINGLE_CHANNEL_DMM"
```

## Historical gate runners

Gate 3 and Gate 4 fault/performance BAT files remain for archived evidence reproduction. They enforce their original gate and fixture interlocks.
