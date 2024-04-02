
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

## Example
Read 10 samples with 100ms interval (with `main.cpp`):
```
$ ./main -t 100 -n 10
Time,ina226-Curr(A),ina226-Voltage(V),ina226-Power(W)
2024-04-02-18-42-26-478,1.068000,11.885000,12.700000
2024-04-02-18-42-26-579,1.069000,11.886000,12.712000
2024-04-02-18-42-26-680,1.068000,11.885000,12.700000
2024-04-02-18-42-26-781,1.070000,11.885000,12.712000
2024-04-02-18-42-26-882,1.068000,11.885000,12.700000
2024-04-02-18-42-26-983,1.070000,11.886000,12.725000
2024-04-02-18-42-27-084,1.071000,11.885000,12.700000
2024-04-02-18-42-27-185,1.068000,11.885000,12.687000
2024-04-02-18-42-27-285,1.070000,11.886000,12.725000
2024-04-02-18-42-27-386,1.070000,11.886000,12.712000
```

Read 10 samples with 50ms interval (with `main2.cpp`):
```
$ ./main2 -t 50 -n 10
Time,ina226-in1(V),ina226-in2(V),ina226-power1(W),ina226-curr1(A)
2024-04-02-18-43-42-954,0.005000,11.878000,12.950000,1.089000
2024-04-02-18-43-43-006,0.005000,11.885000,12.700000,1.068000
2024-04-02-18-43-43-057,0.005000,11.886000,12.725000,1.071000
2024-04-02-18-43-43-108,0.005000,11.886000,12.725000,1.070000
2024-04-02-18-43-43-159,0.005000,11.885000,12.700000,1.068000
2024-04-02-18-43-43-211,0.005000,11.885000,12.700000,1.068000
2024-04-02-18-43-43-262,0.005000,11.886000,12.725000,1.071000
2024-04-02-18-43-43-313,0.005000,11.886000,12.712000,1.069000
2024-04-02-18-43-43-364,0.005000,11.885000,12.700000,1.068000
2024-04-02-18-43-43-416,0.005000,11.885000,12.712000,1.069000
```
