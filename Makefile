
# Makefile

main: main.cpp
	g++ -o main -Os -Wall -std=c++17 main.cpp

main2: main2.cpp
	g++ -o main2 -Os -Wall -std=c++17 main2.cpp -lsensors
