# Internal Robot support package

This Python package is the implementation layer used by `EResistorRobotLibrary`. It is not a separately exposed regression workflow.

- `clients.py`: HTTP and SCPI clients with complete transcripts.
- `hil.py`: RP2040 USB CDC capture, USB/VISA DMM control, settling logic, resistance math, and physical evidence.
- `parsers.py`: HTTP/SCPI state, calibration, and key/value parsers.
- `suite.py`: test implementations called one-to-one by Robot cases.
- `coverage.py`: requirements traceability and coverage output.
- `evidence.py`: completion markers, SHA-256 manifests, and verification.
- `reports.py`: JSON, JUnit, CSV, and Markdown report writers.
- `logging_ext.py`: event and protocol logging.

Active output tests remain blocked unless Robot supplies explicit authorization and the exact single-channel DMM fixture confirmation.
