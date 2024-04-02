
# Simple power monitor tool for Xilinx ZCU104

This repository contains two variations (`main.cpp` and `main2.cpp`).
Tested on Xilinx ZCU104 (with Pynq environment).

- `main.cpp` is based on the following GitHub repository:
  - https://github.com/jparkerh/xilinx-linux-power-utility
- `main2.cpp` depends on the `lm-sensors` library:
  - https://github.com/lm-sensors/lm-sensors

- `main.cpp` reads the current, voltage, and power from the hwmon drivers
and formats them in a CSV format.
It first traverses the directory `/sys/class/hwmon/ina*` to scan all available
INA power monitors on the board.
- `main2.cpp` uses `lm-sensors` to scan the INA power monitors instead of
traversing `/sys/class/hwmon/ina*`.

## Usage
Install `lm-sensors` library first (if you use `main2.cpp`):
```
$ sudo apt install libsensors4-dev
```

Build:
```
$ make main
$ make main2
```

`main` and `main2` provide the same command-line options:
```
$ ./main -h
Simple power monitor tool

Usage (example):
./main > out.csv 2>&1
./main 2>&1 | tee out.csv

Options:
-n [N]
    Number of samples (default: -1)
    Negative value is to run the program infinitely
-t [Interval]
    Period between samples in milliseconds (default: 100)
-l
    List all found INA devices and exit
```
