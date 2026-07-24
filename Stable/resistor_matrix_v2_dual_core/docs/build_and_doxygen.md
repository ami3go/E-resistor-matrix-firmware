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
resistor_matrix_v1_dual_core.ino
```

The documentation files and Doxyfile are ignored by the Arduino build.
