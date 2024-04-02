
// main2.cpp

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// For getopt()
#include <unistd.h>

// libsensor
#include <sensors/sensors.h>

struct INASensor
{
  const sensors_chip_name* name_;
  int feat_nr_;
  const char* unit_;
  std::string label_;
};

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

  constexpr const char* kPrefix = "ina";

  // Get all detected chips
  int num_chips = 0;
  const sensors_chip_name* chip_name;
  while ((chip_name = sensors_get_detected_chips(nullptr, &num_chips))) {
    // std::cerr << "Name: " << chip_name->prefix << ", "
    //           << "Path: " << chip_name->path << '\n';

    if (std::strncmp(chip_name->prefix, kPrefix, std::strlen(kPrefix)) != 0)
      continue;

    int num_feats = 0;
    const sensors_feature* feat;
    while ((feat = sensors_get_features(chip_name, &num_feats))) {
      // std::cerr << "Feature Name: " << feat->name << '\n';
      // char* label = sensors_get_label(chip_name, feat);
      // std::free(label);

      std::string label_str = std::string(chip_name->prefix) + "-" + feat->name;
      const sensors_subfeature* subfeat = nullptr;
      const char* unit = nullptr;

      if (feat->type == sensors_feature_type::SENSORS_FEATURE_CURR) {
        subfeat = sensors_get_subfeature(chip_name, feat,
          sensors_subfeature_type::SENSORS_SUBFEATURE_CURR_INPUT);
        unit = "A";
      } else if (feat->type == sensors_feature_type::SENSORS_FEATURE_IN) {
        subfeat = sensors_get_subfeature(chip_name, feat,
          sensors_subfeature_type::SENSORS_SUBFEATURE_IN_INPUT);
        unit = "V";
      } else if (feat->type == sensors_feature_type::SENSORS_FEATURE_POWER) {
        subfeat = sensors_get_subfeature(chip_name, feat,
          sensors_subfeature_type::SENSORS_SUBFEATURE_POWER_INPUT);
        unit = "W";
      }

      if (!subfeat)
        continue;

      INASensor sensor;
      sensor.name_ = chip_name;
      sensor.feat_nr_ = subfeat->number;
      sensor.unit_ = unit;
      sensor.label_ = label_str;
      sensors.push_back(sensor);
    }
  }

  return sensors;
}

// List the sensors
void ListINASensors(const std::vector<INASensor>& sensors)
{
  for (const auto& sensor : sensors) {
    std::cerr << "Path: " << sensor.name_->path << ", "
              << "Name: " << sensor.label_ << '\n';
  }
}

// Monitor the sensors
void MonitorINASensors(const Config& conf,
                       const std::vector<INASensor>& sensors)
{
  // Write the header once
  std::vector<std::string> values;

  values.push_back("Time");

  for (const auto& sensor : sensors)
    values.push_back(sensor.label_ + "(" + sensor.unit_ + ")");

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
      double value;
      if (sensors_get_value(sensor.name_, sensor.feat_nr_, &value) != 0)
        values.push_back("");
      else
        values.push_back(std::to_string(value));
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

  // Initialize libsensors
  if (sensors_init(nullptr) != 0) {
    std::cerr << "sensors_init() failed\n";
    std::exit(EXIT_FAILURE);
  }

  // Scan the INA power sensors
  const auto sensors = ScanINASensors();

  // List all available INA sensors
  if (conf.list_) {
    ListINASensors(sensors);
    sensors_cleanup();
    return EXIT_SUCCESS;
  }

  // Monitor the sensors
  MonitorINASensors(conf, sensors);

  // Cleanup libsensors
  sensors_cleanup();

  return EXIT_SUCCESS;
}
