# Gate 2 build and test sequence

```bat
setup_arduino_cli_environment.bat
arduino_cli_doctor.bat
build_firmware.bat
build_and_flash_COM17.bat
```

After the device returns, run the Gate 2 regression package:

```bat
run_python_read_only.bat G2
run_python_safe_output.bat G2
run_python_hil_single_channel.bat G2
```

Do not promote to Gate 3 until all Gate 2 reports pass and the final device state contains eight `0000` masks.
