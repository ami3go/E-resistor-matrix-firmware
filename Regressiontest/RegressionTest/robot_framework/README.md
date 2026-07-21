# E-Resistor Robot Framework runner

The supported profiles are:

```text
read_only
safe_output
hil_single_channel
gate3_transport_fault
```

Use the top-level Windows launchers for normal bench operation. Direct launcher syntax is available with:

```powershell
.\.venv\Scripts\python.exe .\robot_framework\run_robot.py --help
```

Gate 4 targets firmware 0.7.0. The inherited Gate 3 transport tests remain active, while Gate 4 adds profile timing, coherent snapshot, and rollback-fault coverage. The fault profile is accepted only with explicit active-HIL authorization and the exact single-channel fixture confirmation token.

The internal Python library is an implementation detail of Robot Framework. It provides Ethernet/SCPI clients, parser logic, DMM and serial capture, coverage traceability, reports, and evidence finalization.
