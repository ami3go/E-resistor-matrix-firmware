# CYD ST7789V + XPT2046 TFT/Touch Test

Simple Arduino IDE test sketch for your CYD board.

It uses:

```text
LovyanGFX only
No LVGL
No LVGL_CYD
No generator UART code
```

## Hardware config

TFT:

```text
Controller: ST7789V
SCLK: GPIO14
MOSI: GPIO13
CS:   GPIO15
DC:   GPIO2
Backlight: GPIO21
SPI mode: 3
Speed: 40 MHz
Color order: BGR
Resolution: 320 x 240
```

Touch:

```text
Controller: XPT2046
SCLK: GPIO25
MOSI: GPIO32
MISO: GPIO39
CS:   GPIO33
IRQ:  GPIO36
Speed: 2 MHz
Calibration:
  x_min: 390
  x_max: 3800
  y_min: 210
  y_max: 3800
```

## Install

Install Arduino library:

```text
LovyanGFX
```

No LVGL needed for this test.

## Flash

Open:

```text
CYD_ST7789V_XPT2046_TFT_TOUCH_TEST.ino
```

Board:

```text
ESP32 Dev Module
```

or your CYD board if available.

Serial Monitor:

```text
115200 baud
```

## Expected result

1. Screen shows red/green/blue/white bars.
2. Screen changes to black test screen.
3. Touching the screen draws red/yellow markers.
4. Serial Monitor prints:

```text
Touch x=123 y=145
```

## If screen is white

Edit the `.ino` file:

```cpp
#define TFT_SPI_WRITE_HZ 40000000
```

Try:

```cpp
#define TFT_SPI_WRITE_HZ 2000000
```

Then try:

```cpp
#define TFT_SPI_WRITE_HZ 10000000
```

## If colors are wrong

Change:

```cpp
#define ST7789_BGR_ORDER true
```

to:

```cpp
#define ST7789_BGR_ORDER false
```

## If orientation is wrong

Change:

```cpp
#define DISPLAY_ROTATION 1
```

Try values:

```cpp
0
1
2
3
```

## If touch is mirrored or rotated

First check if x/y values print in Serial Monitor.

If touch works but coordinates are wrong, adjust the calibration:

```cpp
#define TOUCH_X_MIN 390
#define TOUCH_X_MAX 3800
#define TOUCH_Y_MIN 210
#define TOUCH_Y_MAX 3800
```

or try changing:

```cpp
#define DISPLAY_ROTATION 1
```

