# Robot Framework implementation summary

Version **2.8.1** exposes seven Robot suites:

| Profile | Purpose |
|---|---|
| `read_only` | Identity, state, calibration, GUI, diagnostics, latency, heap and protocol compatibility |
| `safe_output` | Zero-mask and all-off command transport |
| `hil_single_channel` | Full selected-channel DMM/USB-COM HIL; Gate 5 adds concurrent service polling |
| `gate5_service` | Gate 5 HTTP/SCPI/API/heap/performance acceptance |
| `gate3_transport_fault` | Historical Gate 3 generation/deadline fault injection |
| `gate4_profile` | Historical Gate 4 two-phase performance validation |
| `gate4_profile_fault` | Historical Gate 4 post-clear failure safety validation |

Robot tests invoke the shared internal Python regression methods; they do not duplicate protocol or HIL logic. The package intentionally exposes no pure-Python user runner or Arduino source-build workflow.
