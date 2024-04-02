
// main.cpp

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// For getopt()
#include <unistd.h>

namespace fs = std::filesystem;

struct INASensor
{
  fs::path path_base_;
  std::string name_;
  std::optional<fs::path> path_current_;
  std::optional<fs::path> path_voltage_;
  std::optional<fs::path> path_power_;
};

bool operator<(const INASensor& lhs, const INASensor& rhs)
{
  return lhs.path_base_ < rhs.path_base_;
}

struct Config
{
  // Interval (ms)
  int interval_;
  // Iterations
  int max_iter_;
  // List the sensors and exit
  bool list_;

  // Constructor
  Config() : interval_(100),
             max_iter_(-1),
             list_(false) { }
};

// Get a timestamp (with milliseconds)
std::string GetTimestamp()
{
  const auto now = std::chrono::system_clock::now();
  const auto now_t = std::chrono::system_clock::to_time_t(now);
  const auto now_ms = std::chrono::duration_cast<
    std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

  std::stringstream buffer;
  buffer << std::put_time(std::localtime(&now_t), "%Y-%m-%d-%H-%M-%S")
         << "-" << std::setfill('0') << std::setw(3) << now_ms.count();

  return buffer.str();
}

// Join with the delimiter
// https://stackoverflow.com/questions/5288396
template <typename T>
std::string Join(const T& values, const char* delimiter)
{
  using V = typename T::value_type;

  std::ostringstream buffer;
  auto b = std::begin(values);
  auto e = std::end(values);

  if (b != e) {
    std::copy(b, std::prev(e), std::ostream_iterator<V>(buffer, delimiter));
    b = std::prev(e);
  }

  if (b != e) {
    buffer << *b;
  }

  return buffer.str();
}

// Read the file content (does not check the existence of the file)
template <typename T>
T ReadSensor(const fs::path& path)
{
  std::ifstream f { path };
  T x;
  f >> x;
  f.close();
  return x;
}

// Print the usage
void PrintUsage(char** argv)
{
  std::cerr << "Simple power monitor tool\n\n";
  std::cerr << "Usage (example):\n"
            << argv[0] << " > out.csv 2>&1\n"
            << argv[0] << " 2>&1 | tee out.csv\n\n";

  std::cerr << "Options:\n"
            << "-n [N]\n"
            << "    Number of samples (default: -1)\n"
            << "    Negative value is to run the program infinitely\n"
            << "-t [Interval]\n"
            << "    Period between samples in milliseconds (default: 100)\n"
            << "-l\n"
            << "    List all found INA devices and exit\n";
}

// Get the command-line options
Config GetConfig(int argc, char** argv)
{
  int opt;
  Config conf;

  opterr = 0;

  while ((opt = getopt(argc, argv, "n:t:l")) != -1) {
    switch (opt) {
      case 't':
        conf.interval_ = std::atoi(optarg);

        if (conf.interval_ < 0) {
          std::cerr << "Invalid interval: " << conf.interval_ << "ms\n";
          std::exit(EXIT_FAILURE);
        }
        break;

      case 'n':
        // Negative value indicates the infinite loop
        conf.max_iter_ = std::atoi(optarg);
        break;

      case 'l':
        conf.list_ = true;
        break;

      case '?':
        PrintUsage(argv);
        std::exit(EXIT_FAILURE);
        break;
    }
  }

  return conf;
}

// Scan the INA power sensors
std::vector<INASensor> ScanINASensors()
{
  std::vector<INASensor> sensors;

  // Scan /sys/class/hwmon/*
  constexpr const char* kPath = "/sys/class/hwmon";
  for (const fs::directory_entry& dir_sensor : fs::directory_iterator(kPath)) {
    if (!fs::is_directory(dir_sensor.path()))
      continue;

    INASensor sensor;
    sensor.path_base_ = dir_sensor.path();

    // Read the sensor name
    const fs::path path_sensor_name = dir_sensor.path() / "name";
    if (fs::is_regular_file(path_sensor_name))
      sensor.name_ = ReadSensor<std::string>(path_sensor_name.c_str());

    // Sensor name should start with `ina`
    if (sensor.name_.rfind("ina", 0) != 0)
      continue;

    // Construct the file paths to read current, voltage, and power
    const fs::path path_current = dir_sensor.path() / "curr1_input";
    if (fs::is_regular_file(path_current))
      sensor.path_current_ = path_current;

    const fs::path path_voltage = dir_sensor.path() / "in2_input";
    if (fs::is_regular_file(path_voltage))
      sensor.path_voltage_ = path_voltage;

    const fs::path path_power = dir_sensor.path() / "power1_input";
    if (fs::is_regular_file(path_power))
      sensor.path_power_ = path_power;

    // Skip of any of these is not available
    if (!(sensor.path_current_ || sensor.path_voltage_ || sensor.path_power_))
      continue;

    sensors.push_back(sensor);
  }

  // Sort the INA sensors
  std::sort(sensors.begin(), sensors.end());

  return sensors;
}

// List the sensors
void ListINASensors(const std::vector<INASensor>& sensors)
{
  for (const auto& sensor : sensors) {
    std::cerr << "Path: " << sensor.path_base_.c_str() << ", "
              << "Name: " << sensor.name_ << '\n';
  }
}

// Monitor the sensors
void MonitorINASensors(const Config& conf,
                       const std::vector<INASensor>& sensors)
{
  // Write the header once
  std::vector<std::string> values;

  values.push_back("Time");

  for (const auto& sensor : sensors) {
    if (sensor.path_current_)
      values.push_back(sensor.name_ + "-Curr(A)");
    if (sensor.path_voltage_)
      values.push_back(sensor.name_ + "-Voltage(V)");
    if (sensor.path_power_)
      values.push_back(sensor.name_ + "-Power(W)");
  }

  std::cerr << Join(values, ",") << '\n';

  values.clear();
  int iter = 0;

  while (1) {
    if (conf.max_iter_ >= 0 && iter++ >= conf.max_iter_)
      break;

    // Get the current time
    values.push_back(GetTimestamp());

    for (const auto& sensor : sensors) {
      // Read the current, voltage, and power
      if (sensor.path_current_)
        values.push_back(std::to_string(
          ReadSensor<int>(*sensor.path_current_) / 1e3));
      if (sensor.path_voltage_)
        values.push_back(std::to_string(
          ReadSensor<int>(*sensor.path_voltage_) / 1e3));
      if (sensor.path_power_)
        values.push_back(std::to_string(
          ReadSensor<int>(*sensor.path_power_) / 1e6));
    }

    std::cerr << Join(values, ",") << '\n';
    values.clear();

    std::this_thread::sleep_for(std::chrono::milliseconds(conf.interval_));
  }
}

int main(int argc, char** argv)
{
  // Get the command-line options
  const auto conf = GetConfig(argc, argv);

  // Scan the INA power sensors
  const auto sensors = ScanINASensors();

  if (sensors.empty()) {
    std::cerr << "Power monitor is not found\n";
    std::exit(EXIT_FAILURE);
  }

  // List all available INA sensors
  if (conf.list_) {
    ListINASensors(sensors);
    return EXIT_SUCCESS;
  }

  // Monitor the sensors
  MonitorINASensors(conf, sensors);

  return EXIT_SUCCESS;
}
