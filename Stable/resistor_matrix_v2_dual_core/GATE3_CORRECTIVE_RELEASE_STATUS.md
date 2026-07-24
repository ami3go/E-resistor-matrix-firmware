# Gate 3 corrective release status

- Firmware: **0.6.2**
- Gate: **G3 corrective candidate**
- Status: **implementation complete; hardware revalidation required**

The uploaded 0.6.0 report cannot close G3 because persistent calibration was absent, USB CDC DTR was deasserted by the harness, G3-specific production cases were absent from that older harness execution, and no fault-injection result was included.

Firmware 0.6.2 preserves the Gate 3 deterministic Core 1 transport and adds calibration/USB observability and recovery.
