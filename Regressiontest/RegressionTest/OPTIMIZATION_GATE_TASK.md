# E-Resistor Firmware Optimization Program

## Purpose

Refactor and optimize the E-Resistor dual-core RP2040 firmware in small, independently releasable gates. Each gate must preserve public behavior unless the gate explicitly introduces a versioned API change. A gate may be merged only after its automated regression suite passes and its performance, memory, and safety measurements are compared with the preceding accepted baseline.

Reference firmware: **v0.4.4**.

## Mandatory development rules

1. Implement one gate at a time. Do not combine unrelated gates into one release.
2. Tag every accepted gate and preserve its regression report.
3. Start each gate from the last accepted gate, not from an unreviewed development branch.
4. Keep outputs OFF before storage, OTA, fault-injection, or destructive tests.
5. No optimization may weaken break-before-make behavior, mask safety validation, calibration fidelity, or command error reporting.
6. New cross-core communication must use fixed-size data and explicit ownership.
7. Human-readable strings, `String`, LittleFS, HTTP, SCPI parsing, and Serial formatting must remain on Core 0.
8. Core 1 must never wait for a command that it submitted to itself.
9. Every potentially blocking operation must have a measured deadline and progress instrumentation before watchdog activation.
10. A failed gate must be rolled back or corrected before the next gate begins.

## Versioning proposal

| Gate | Suggested version | Scope |
|---|---:|---|
| G0 | 0.4.5-test | Baseline and test infrastructure |
| G1 | 0.4.6 | Low-risk cleanup and observability |
| G2 | 0.5.0 | Numeric resistor model and target-search optimization |
| G3 | 0.6.2 | Dual-core ownership and command transport |
| G4 | 0.7.2 | Two-phase multi-channel switching |
| G5 | 0.8.0 | HTTP and SCPI restructuring |
| G6 | 0.9.0 | Atomic storage and streamed calibration |
| G7 | 1.0.0-rc1 | Transactional single-copy OTA |
| G8 | 1.0.0-rc2 | Watchdog and recovery mode |
| G9 | 1.0.0 | Legacy removal and release hardening |

# Gate G0 — Baseline and regression infrastructure

## Objective

Create a reproducible baseline before changing runtime behavior.

## Required implementation

- Add a pinned Arduino-Pico core version and library inventory.
- Add an Arduino CLI build script or equivalent reproducible build command.
- Enable compiler warnings suitable for the selected core.
- Add build metadata to the firmware identity response.
- Add a read-only capability query, preferably `SYST:CAPS?`.
- Add operation timing counters for:
  - Single-channel apply.
  - All-off.
  - Eight-channel profile apply.
  - Target-mask search.
  - HTTP `/state` generation.
  - Calibration import.
  - LittleFS write.
  - OTA stages when they are later implemented.
- Record firmware binary size, static RAM, free heap, loop rate, maximum loop busy time, and both core loop counters.
- Integrate the supplied Python regression harness.
- Establish the single-channel HIL fixture using Ethernet SCPI, RP2040 USB serial capture, and a USB/VISA DMM.

## Baseline measurements

Capture at least:

- Firmware binary size.
- Static RAM estimate from the build output.
- Free heap after boot and after 500 `/state` requests.
- Average, p95, p99, and maximum HTTP ping latency.
- Average, p95, p99, and maximum `/state` latency.
- Average and maximum SCPI `*IDN?` latency.
- Calibration response size and parse time.
- Single-channel zero-mask apply latency.
- Eight-channel zero-profile apply latency.
- Core 1 queue overflow count.
- Physical resistance for all 16 individual bits on the selected fixture channel.
- Physical resistance for the configured safe combination masks.
- Selected-channel OFF resistance and repeated-switching repeatability.

## Exit criteria

- Read-only regression profile passes.
- Safe-output regression profile passes with all masks zero.
- `hil_single_channel` regression passes on the real bench and its measurement CSV is archived.
- Reports are generated as JSON, JSONL, CSV, JUnit XML, and Markdown.
- A baseline summary file is archived for comparison by G1.
- No functional change other than instrumentation.

# Gate G1 — Low-risk cleanup and observability

## Objective

Remove obvious prototype overhead and repair small correctness issues without changing the data model or architecture.

## Required implementation

- Remove all `Serial.print`, `Serial.println`, and `Serial.flush` calls from Core 1 production paths.
- Replace Core 1 logging with compact numeric events consumed and formatted by Core 0.
- Add compile-time log levels. Production builds must not emit per-bit or per-latch logs.
- Make `forceAllOff()` return a verified success/failure result and preserve the real error.
- Stop HTTP and SCPI handlers from clearing an all-off failure.
- Fix SCPI oversized-line handling by discarding input until newline.
- Enable a separate Core 1 stack and measure stack margin.
- Replace remaining magic channel/bit limits with `CHANNEL_COUNT` and `BIT_COUNT`.
- Correct the intended default IP in one centralized configuration definition.
- Remove obsolete split-from-INO and historical implementation comments from source files; preserve history in documentation.
- Add source checks preventing Core 1 Serial regressions.

## Performance target

- Single-channel apply p95 must improve or remain within 5% of G0.
- Profile apply p95 must improve or remain within 5% of G0.
- Core 1 maximum loop duration must not regress by more than 5%.
- Firmware image must not grow by more than 3% without documented justification.

## Exit criteria

- All G0 tests pass.
- New SCPI overflow test passes.
- All-off error propagation tests pass.
- Core 1 source scan contains no production Serial calls.

# Gate G2 — Numeric resistor model and search optimization

## Objective

Replace runtime text-based resistor data with a compact numeric model and optimize target search without changing calculated results.

## Required implementation

- Replace per-entry `bit`, MOSFET text, and resistance text storage with numeric resistance values indexed directly by channel and bit.
- Store or generate MOSFET names once; do not duplicate them per channel.
- Parse strings only at import and display boundaries.
- Eliminate the linear runtime lookup by bit.
- Consolidate duplicated default channel tables when the defaults are identical.
- Retain a conductance representation only if benchmarks prove it beneficial.
- Remove per-candidate `logf()` from target search; compare candidate error using multiplication/cross-products or another mathematically verified method.
- Add a target-search deadline, candidate counter, elapsed time, and cancellation checkpoint.
- Add a read-only dry-run command, for example:
  - `CH<n>:TARGET:CALC? <ohm>`
  - Response: requested value, selected mask, calculated value, absolute error, percent error, candidates, elapsed microseconds.
- Keep existing calibration import/export formats compatible.

## Regression oracle

Before replacing the old model, export a deterministic vector set containing:

- All 16 single-bit masks for every channel.
- Zero mask for every channel.
- At least 1,000 deterministic random masks per channel.
- Target searches across the full supported range.

The new implementation must match the old implementation within the selected floating-point tolerance and must select an equivalent or better mask.

## Performance target

- Save at least 2 KB static/runtime RAM compared with G1, or document why the measured saving differs.
- Normal four-bit target search p95 must improve by at least 20%.
- Expert full-mask search must complete inside the configured deadline.

## Exit criteria

- Calibration table parser tests pass.
- Numeric equivalence tests pass on every channel.
- Dry-run target tests pass without changing outputs.
- Heap stability is not worse than G1.

# Gate G3 — Dual-core ownership and command transport

## Objective

Make Core 1 a deterministic physical-output engine with no dependency on UI, storage, parser, or human-readable state.

## Required implementation

- Core 0 owns:
  - HTTP and SCPI.
  - LittleFS and OTA.
  - Calibration parsing and configuration installation.
  - Safety-policy calculation.
  - Human-readable status, errors, and logs.
- Core 1 owns:
  - Shift-register GPIO/PIO.
  - Physical break-before-make sequences.
  - Authoritative output masks and apply counters.
  - Direct physical all-off on Core 0 failure.
- Introduce compact fixed-size command/result structures with numeric status codes.
- Add command sequence, deadline, and safety generation fields.
- Reject expired and invalidated commands before touching hardware.
- Publish an atomic output-state snapshot to Core 0.
- Install immutable calibration/safety snapshots only while outputs are OFF.
- Split Core 0 and Core 1 safe-state functions. Core 1 must call physical all-off directly.
- Replace response busy polling with a semaphore, event, Pico SDK queue, or another bounded notification mechanism.
- Add startup synchronization; remove timing assumptions such as fixed delays used as inter-core handshakes.

## Test-build fault hooks

Under `ERESISTOR_TEST_MODE` only, add guarded commands for:

- Delaying one Core 1 command before execution.
- Invalidating a queued command generation.
- Pausing Core 1 processing for a bounded period.
- Reading queue and rejection counters.

Hooks must require all outputs OFF and must not exist in production builds.

## Exit criteria

- No late/ghost actuation after a command timeout.
- Core 1 source files contain no `String`, LittleFS, HTTP, SCPI parser, or human-readable status formatting.
- Command stress test completes without queue overflow or state mismatch.
- Output snapshot and SCPI/HTTP state remain consistent during concurrent polling.

# Gate G4 — Two-phase multi-channel switching

## Objective

Improve profile transition speed and eliminate mixed old/new channel states.

## Required implementation

Implement a two-phase profile transition:

1. Validate the complete profile on Core 0.
2. Latch zero to all channels.
3. Wait one global break-before-make interval.
4. Latch all requested masks.
5. Publish one coherent output-state snapshot.
6. On any failure, directly latch all channels OFF.

Keep the existing bit-banged backend for this gate. PIO or hardware SPI is a separate optional follow-up after behavior is proven.

## Performance target

- Eight-channel profile transition p95 must improve by at least 30% relative to G3.
- One global break-before-make interval must replace eight repeated intervals.
- There must be no externally reported mixed profile state.

## Exit criteria

- Zero-profile stress test passes for at least 1,000 cycles.
- Optional hardware logic-analyzer test confirms all channels are cleared before any new mask is enabled.
- Failure injection returns all channels to zero.

# Gate G5 — HTTP and SCPI restructuring

## Objective

Reduce heap fragmentation, parser allocation, source coupling, and duplicated documentation.

## Required implementation

- Split the monolithic HTTP module into control, calibration, maintenance, diagnostics, API, and page-writer modules.
- Split the central application header into focused headers.
- Move static CSS and JavaScript into flash-backed, cacheable resources.
- Stream large HTTP responses through a fixed-size buffer instead of building complete 8–24 KB `String` objects.
- Capture one atomic state snapshot before formatting a response.
- Replace allocation-heavy SCPI parsing with a fixed-buffer token parser or command table.
- Generate SCPI help/table documentation from the same command registry used by the parser.
- Convert state-changing HTTP GET routes to POST.
- Introduce `/api/v1/` endpoints while preserving documented compatibility aliases during a deprecation period.
- Split the large state response into focused endpoints where practical.

## Performance target

- Peak temporary heap used while generating the Calibration page must fall by at least 50%.
- `/state` p95 latency must not regress by more than 5%.
- Repeating page requests 1,000 times must not cause a persistent free-heap decline beyond the configured threshold.

## Exit criteria

- Browser page-content smoke tests pass.
- SCPI alias compatibility tests pass.
- HTTP mutation-method tests pass.
- API v1 schema tests pass.

# Gate G6 — Atomic storage and streamed calibration

## Objective

Unify LittleFS access and make all persistent configuration changes transactional.

## Required implementation

- Create a common storage service for temporary write, validation, backup, rename, reopen, and recovery.
- Apply atomic writes to network, safety, profile, metadata, calibration, and watchdog-stat files.
- Add schema version and checksum/CRC where appropriate.
- Automatically migrate legacy safety/config formats and rewrite them in the current format after successful loading.
- Stream calibration imports rather than retaining multiple full `String` copies.
- Parse directly into numeric candidate tables.
- Keep old runtime tables active until all new tables have been validated and installed.
- Leave affected channels OFF after calibration replacement.
- Add recovery of interrupted `.tmp`/`.bak` transactions at boot.

## Exit criteria

- Power-loss simulation tests pass at every transaction stage.
- Corrupt-file tests fall back to the last known-good file.
- Calibration bundle round-trip is byte/semantic equivalent.
- No partial channel-table update is possible.

# Gate G7 — Transactional single-copy OTA

## Objective

Replace double-copy OTA with one staged candidate while preserving the installed firmware until validation and commit succeed.

## Required implementation

- Upload to one candidate file only.
- Reject short writes and images larger than the current safe staging limit.
- Validate size, board identity, flash layout, SHA-256, and cryptographic signature.
- Reopen and verify the staged file before commit.
- Commit the OTA command only after every validation succeeds.
- Reboot only after commit succeeds.
- Delete or quarantine failed candidates without changing the running image.
- Keep all outputs verified OFF and reject output commands for the complete OTA maintenance state.
- Add progress instrumentation and no-progress/total deadlines.
- Report each OTA stage and failure reason through GUI, SCPI, and exported logs.
- Document that standard single-slot OTA protects transfer/flash interruption but does not roll back a valid yet defective new firmware image.

## Test profiles

- Invalid extension.
- Truncated upload.
- Oversized image.
- Wrong SHA-256.
- Invalid signature.
- Simulated LittleFS short write.
- Commit failure.
- Network disconnect during upload.
- Valid update and reboot.

OTA tests must require explicit user flags and a known recovery method.

## Exit criteria

- Old firmware remains bootable after every pre-commit failure.
- Only one full candidate image exists in LittleFS.
- Valid OTA completes with measured progress and no watchdog support yet.

# Gate G8 — Watchdog and recovery mode

## Objective

Enable production watchdog supervision only after long-running operations and dual-core ownership have been corrected.

## Required implementation

- Hardware watchdog timeout near 8 seconds.
- Only Core 0 feeds the hardware watchdog.
- Core 0 supervises Core 1 heartbeat.
- Core 1 supervises Core 0 heartbeat and directly disables outputs on Core 0 failure.
- Maintenance-mode watchdog checkpoints feed only on verified forward progress.
- OTA progress callbacks and resistance-search checkpoints must be integrated.
- Capture reset reason and watchdog counters.
- Detect repeated watchdog resets and enter recovery mode.
- Recovery mode keeps outputs OFF while diagnostics and signed OTA remain available.
- Add a live Watchdog page and read-only SCPI status commands.
- Add test-only bounded fault injection.

## Exit criteria

- Core 0 freeze, Core 1 freeze, dual freeze, long calculation, LittleFS operation, and OTA scenarios pass.
- No false reset during normal 24-hour stress operation.
- Recovery mode activates after the configured consecutive reset threshold.

# Gate G9 — Legacy removal and production release

## Objective

Remove expired compatibility paths and produce the first production candidate.

## Required implementation

- Remove deprecated HTTP and SCPI aliases after confirming PC-driver migration.
- Remove legacy global safety fields from the versioned API.
- Move C-header calibration conversion to the PC tool and remove it from production firmware if no longer needed.
- Remove unused variables, dead code, stale comments, and placeholder pages.
- Complete Doxygen and architecture documentation.
- Add CI build, unit tests, source checks, firmware-size limits, and report publishing.
- Run a complete 24-hour soak test and hardware-in-loop regression.
- Freeze the API and configuration schema for v1.0.

## Release criteria

- All regression profiles pass.
- No critical or high-severity review finding remains open.
- No compiler warnings remain without documented justification.
- Firmware size and LittleFS headroom satisfy defined release margins.
- Recovery and update procedures are documented and tested.

# Regression execution policy

## Required profiles

### `read_only`

Safe for an attached DUT. Queries identity, state, calibration, pages, counters, and performance.

### `safe_output`

Requires explicit `--allow-output-tests`. Sends only zero masks and all-off commands. It exercises command queues without enabling a resistor branch.

### `active_output`

Reserved for custom multi-channel fixtures. Requires explicit `--allow-active-output-tests`, known safe masks, and automatic all-off cleanup. Never run on an unknown DUT.

### `hil_single_channel`

Uses the available real-hardware fixture: Ethernet SCPI for control, RP2040 USB COM for independent firmware logs, and a USB/VISA DMM connected across one selected E-Resistor channel. It requires both:

```text
--allow-active-output-tests
--fixture-confirmation E_RESISTOR_SINGLE_CHANNEL_DMM
```

It performs an all-off precheck, a 16-bit physical walk, configured combination measurements, repeated switching, serial fault inspection, and a final all-off isolation measurement. The other seven channel masks must remain `0000` during every active measurement.

### `storage`

Requires explicit `--allow-storage-tests`. Uses backed-up test files and restores the original configuration.

### `ota`

Requires explicit `--allow-ota-tests`, a valid signed test image, and a recovery procedure.

### `watchdog`

Requires explicit `--allow-watchdog-tests`, test-mode firmware, and physical access.

# Mandatory HIL execution matrix

The real single-channel HIL profile is part of gate acceptance, not an optional demonstration. Detailed setup and commands are defined in `HIL_REGRESSION_TEST_PLAN.md`.

| Gate | Required HIL execution | Additional acceptance focus |
|---|---|---|
| G0 | Full bit walk, combinations, repeated switching | Establish physical and timing baseline |
| G1 | Full HIL | Confirm logging cleanup does not alter switching or accuracy |
| G2 | Full HIL | Numeric model must reproduce calibration and physical resistance |
| G3 | Full HIL with increased repeat cycles | Detect delayed/ghost commands and dual-core transport faults |
| G4 | Full selected-channel HIL plus eight-channel software profile tests | Confirm global break-before-make behavior |
| G5 | Full HIL while HTTP and SCPI stress polling runs | Confirm service refactor does not disturb physical output |
| G6 | HIL before and after calibration backup/import restoration | Confirm atomic storage preserves physical calibration |
| G7 | HIL before OTA and after successful reboot | Confirm identity, calibration, and physical behavior survive update |
| G8 | Full HIL plus watchdog fault injection | DMM must confirm selected channel returns to OFF |
| G9 | Full HIL, extended repeat cycles, 24-hour soak | Production release evidence |

A gate fails when its required HIL run fails, when another channel becomes active, when final all-off cannot be verified, or when physical error exceeds the configured gate limit.

# Report retention

For every gate, archive:

- Firmware source revision and build metadata.
- Build log and size report.
- Test configuration.
- Console log.
- JSONL event log.
- HTTP and SCPI transcripts.
- Raw state before and after tests.
- Metrics CSV.
- Machine-readable results JSON.
- JUnit XML.
- Markdown summary.
- Baseline comparison.

# Definition of done for each gate

A gate is complete only when:

1. Its implementation requirements are met.
2. All required source, protocol, and gate-specific HIL tests pass.
3. Outputs are OFF at test completion.
4. No unexplained performance or memory regression is present.
5. Documentation is updated.
6. The gate report is reviewed and accepted.
7. A new baseline is archived for the next gate.

# Test coverage and traceability requirement

The optimization program shall maintain `TEST_COVERAGE_TABLE.md` as the master traceability matrix. Each requirement shall identify:

- Coverage ID.
- Applicable gate or gates.
- Functional area and requirement.
- Required regression profile.
- Automatic, HIL, fault-injection, build, source, or manual method.
- Test ID or planned test hook.
- Required hardware.
- Objective acceptance rule.
- Retained evidence files.

Every regression execution shall generate `test_coverage.md`, `test_coverage.csv`, and `test_coverage.json` for the selected gate. Gate acceptance shall review both the ordinary test result and the coverage result.

A gate shall not be accepted when a gate-mandatory requirement is `FAIL`. Any `PARTIAL`, `SKIP`, `NOT_RUN`, `PLANNED`, or `MANUAL` row must either be executed using the required profile or be recorded as an approved limitation with responsible owner and follow-up gate. Planned test hooks must be implemented no later than the gate where they become mandatory.

# Robot Framework regression requirement

Each gate may be executed using either the Python CLI runner or the Robot Framework runner, but accepted release evidence should include Robot output for hardware gates.

Required Robot outputs:

- `output.xml`
- `log.html`
- `report.html`
- `robot_events.jsonl`
- `e_resistor_evidence/<profile>/results.json`
- `e_resistor_evidence/<profile>/metrics.csv`
- `e_resistor_evidence/<profile>/test_coverage.*`
- protocol, serial and DMM transcripts applicable to the profile
- `evidence_manifest.sha256`

The Robot implementation shall not duplicate test logic. It must invoke the same underlying Python regression methods and retain the same test IDs and acceptance limits. Active-output suites must retain explicit authorization and fixture-confirmation gates.
