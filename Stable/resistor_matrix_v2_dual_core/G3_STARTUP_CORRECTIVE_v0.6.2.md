# Gate 3 startup corrective release — firmware v0.6.2

## Failure corrected

Firmware v0.6.1 could stop during boot with:

```text
Core 1 hardware engine did not enter safe state. Network services not started.
SAFE STATE: Core 1 hardware engine not ready
SAFE STATE WARNING: shift-register GPIOs not ready yet
```

The v0.6.1 change moved `initCoreCommandEngine()` behind `Serial.begin()` and a startup delay. Arduino-Pico already initializes USB CDC before it launches Core 1 and before the sketch `setup()` function runs, so this reordering did not improve USB enumeration. It instead widened the Core 0/Core 1 startup race.

A second race was present because both cores could call `initCore1EventQueue()` during startup.

## v0.6.2 corrections

- Initializes the Core 1 event queue and command transport as the first Core 0 setup operation.
- Initializes the event queue before publishing the transport-ready flag.
- Removes Core 1 event-queue initialization so only Core 0 owns initialization.
- Makes Core 1 retry its bounded transport wait rather than permanently leaving startup without signaling readiness.
- Adds cross-core memory barriers around the transport initialized flag.
- Uses the coherent output snapshot flags for Core 0 startup acceptance.
- Latches a Core 1 fault when physical initialization or initial all-OFF fails.
- Preserves early USB CDC availability; Arduino-Pico performs USB initialization before sketch setup.

## Required bench verification

1. Build and flash the production image.
2. Confirm the normal COM port enumerates after BOOTSEL flashing.
3. Confirm serial boot reaches:

```text
Core 1 hardware engine ready; all outputs OFF
```

4. Confirm HTTP and SCPI start at `192.168.0.55`.
5. Run the complete Gate 3 Robot profiles using regression package v2.6.3.
