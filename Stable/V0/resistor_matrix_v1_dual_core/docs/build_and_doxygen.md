# Building Doxygen Documentation

This package is Doxygen-ready. The source files contain `@file`, `@brief`, `@param`, and `@return` comments for the public firmware APIs and internal helper functions.

## Generate HTML documentation

Install Doxygen on the host PC, then run from the sketch folder:

```bash
doxygen Doxyfile
```

Output will be generated under:

```text
docs/doxygen/html/index.html
```

## Arduino build

Open the main `.ino` file in Arduino IDE:

```text
rp2040_w5500_resistor_matrix_v1_dual_core_cal_scpi_webhelp_doxygen.ino
```

The documentation files and Doxyfile are ignored by the Arduino build.
