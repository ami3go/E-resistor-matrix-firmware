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

`SYST:VERS?` and `FIRM:VERS?` return only the firmware version string.
`FIRM:BUILD?` returns the compile date and time.
