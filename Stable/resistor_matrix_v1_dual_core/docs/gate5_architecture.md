# Gate 5 HTTP and SCPI architecture

## Module boundaries

| Module | Responsibility |
|---|---|
| `http_routes.cpp` | Route registry, method policy, API aliases |
| `http_control.cpp` | Output-control handlers and API mutation handlers |
| `http_calibration_page.cpp` | Streamed calibration browser page |
| `http_maintenance.cpp` | Files, logs, backup, network, firmware maintenance |
| `http_diagnostics.cpp` | Atomic legacy `/state` and diagnostics pages |
| `http_api_v1.cpp` | Focused versioned JSON schemas |
| `http_static_assets.cpp` | Flash-backed CSS and JavaScript |
| `http_response_writer.cpp` | 512-byte fixed streaming buffer |
| `http_page_helpers.cpp` | Shared page framing and navigation |
| `scpi_command_registry.cpp` | Exact command/alias registry, parser normalization, generated help |
| `runtime_state_snapshot.cpp` | One coherent Core 0/Core 1 state capture for formatting |

## Compatibility policy

The `/api/v1/` namespace is canonical. Existing calibration endpoints remain aliases during Gate 5. State-changing legacy paths no longer accept GET. They require POST and return a structured 405 response for GET.

## Memory policy

Large responses are emitted incrementally. The response writer owns a 512-byte fixed buffer and finalizes HTTP chunked transfer explicitly. Pages no longer reserve 8–24 KB temporary `String` objects. Temporary-heap telemetry is exported by `/api/v1/diagnostics`, legacy `/state`, and `SYST:STAT?`.

## Snapshot policy

Each state response captures one `RuntimeStateSnapshot` before output formatting. Formatters read masks, counters, safety state, calibration state, transport diagnostics, and HTTP telemetry only from that snapshot, preventing mixed fields from different update points.

## SCPI policy

Exact commands are normalized into a fixed input buffer and dispatched through `ScpiCommandId`. Aliases map to the same command IDs. Human-readable HELP and the browser SCPI table iterate the same command registry. Parameterized calibration and channel commands retain bounded compatibility parsing at the Core 0 protocol boundary.
