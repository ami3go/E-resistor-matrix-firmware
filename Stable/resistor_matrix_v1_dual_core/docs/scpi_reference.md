# SCPI Command Reference

The SCPI server uses raw TCP on port `5025`. Commands are ASCII text lines terminated by LF or CRLF.

## Identification and status

```text
*IDN?
SYST:ERR?
SYST:ERR:CLEAR
STATE?
HELP?
```

## Output control

```text
CH<n>:MASK <hex16>
CH<n>:MASK?
ROUT:CHANnel<n>:MASK <hex16>
ROUT:CHANnel<n>:MASK?
ROUT:ALL:MASK <m1>,<m2>,<m3>,<m4>,<m5>,<m6>,<m7>,<m8>
ALL:OFF
OUTP:ALL OFF
```

## Resistance query/control aliases

```text
CH<n>:RES?
CH<n>:CONF?
```

The firmware-side nearest-mask calculation remains available for browser/manual functions. For automated PC drivers, prefer downloading calibration data and calculating the nearest mask on the PC side.

## Calibration resistor table queries

```text
CAL:RES?
CAL:RESISTORS?
CAL:RES? 3
CAL:RES? CH3
CAL:CHAN1:RES?
CAL:CHANnel1:RESISTORS?
CAL:CH1:TABLE?
```

### One-channel response format

```text
CH1:0,Q16,626.000000;1,Q15,1240.000000;...;15,Q1,20000000.000000
```

### All-channel response format

```text
CH1:...|CH2:...|CH3:...|CH4:...|CH5:...|CH6:...|CH7:...|CH8:...
```

### Branch record format

```text
bit_index,mosfet_name,resistance_ohm
```

A PC driver can parse this data, generate candidate masks, calculate equivalent resistance, and then send the selected mask using `ROUT:CHANnel<n>:MASK`.


## Calibration file readback for GUI tools

These commands let a PC calibration GUI discover and download calibration/config files from the device. The returned data is the active runtime calibration table, so it is useful even when a channel is using compile-time defaults or a RAM-only table.

```text
CAL:FILES?
CAL:FILE? CH1
CAL:FILE? 1
CAL:CHAN1:FILE?
CAL:CHAN1:CONFIG?
CAL:ALL:FILES?
```

`CAL:FILES?` returns a compact one-line list:

```text
CH1,path=/ch1.csv,saved=1,size=123;CH2,path=/ch2.csv,saved=0,size=0;...
```

`CAL:FILE? CH1` returns a BEGIN/END wrapped CSV table:

```text
#BEGIN CH1 path=/ch1.csv saved=1 size=123
bit,mosfet_name,nominal_resistance
0,Q16,626R
...
#END CH1
```

`CAL:ALL:FILES?` returns all eight channel tables using the same BEGIN/END block format, followed by `#END ALL`.

Equivalent HTTP endpoints are:

```text
GET /api/calibration/files
GET /api/calibration/download?ch=1
GET /api/calibration/download_all
GET /calibration_download_all      # browser attachment
POST /calibration_import_all       # browser bundle restore
```

## Firmware identity commands

```text
*IDN?
SYST:VERS?
FIRM:VERS?
FIRM:BUILD?
```

`*IDN?` returns:

```text
OpenBench,E-Resistor,<serial>,<firmware_version>
```

As of Gate 5 r5, `*IDN?` also starts the same temporary bright-blue identify blink used by the web UI. The returned SCPI text remains unchanged for driver compatibility.

`SYST:VERS?` and `FIRM:VERS?` return only the firmware version string.
`FIRM:BUILD?` returns the compile date and time.

## Gate 2 read-only calculation and diagnostics

| Command | Description |
|---|---|
| `CH<n>:TARGET:CALC? <ohm>` | Calculate the nearest safe mask without applying it. Returns requested/calculated resistance, mask, error, candidates, and elapsed microseconds. |
| `SYST:DIAG:SERIAL?` | Emit one structured USB CDC `SERIAL_TEST` event and return `OK,SERIAL_TEST`. |

Example:

```text
CH1:TARGET:CALC? 10000
requested_ohm=10000.000000,mask=....,calculated_ohm=...,absolute_error_ohm=...,error_percent=...,candidates=...,elapsed_us=...
```

## Gate 3 transport and coherent-state diagnostics

| Command | Build | Description |
|---|---|---|
| `SYST:CORE:TRANSPORT?` | Production and test | Returns generation, submitted/completed sequences, queue overflows, timeouts, expired commands, generation rejects, invalid commands, policy installs, and Core 0 fail-safe count. |
| `SYST:CORE:SNAPSHOT?` | Production and test | Returns one atomic snapshot with snapshot sequence, last command sequence, active policy generation, flags, all masks, and apply counters. |
| `SYST:TEST:MODE?` | Test only | Returns `1` when `ERESISTOR_TEST_MODE` is compiled in. |
| `SYST:TEST:CORE1:INVALIDATE:NEXT` | Test only | Arms Core 1 to invalidate the next dequeued command before envelope validation. All outputs must be OFF. |
| `SYST:TEST:CORE1:DELAY <ms>` | Test only | Delays the next Core 1 command by 1–5000 ms. All outputs must be OFF. |
| `SYST:TEST:CORE1:PAUSE <ms>` | Test only | Pauses Core 1 dequeue for 1–5000 ms. All outputs must be OFF. |
| `SYST:TEST:CORE1:INVALIDATE` | Test only | Immediately advances the transport generation. All outputs must be OFF. |

Example production response:

```text
generation=2,last_submitted=104,last_completed=104,command_overflows=0,result_overflows=0,timeouts=0,expired=0,generation_rejects=0,invalid_commands=0,policy_installs=1,core0_failsafe=0
```

Fault hooks are intentionally absent from normal production builds.
