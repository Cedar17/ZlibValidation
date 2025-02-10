// ./src/main.cpp
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include "CLI/CLI.hpp"
#include "nlohmann/json.hpp"
#include "si2dr_liberty.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "lib_file.h"
#include "version.h"

void print_info() {
  spdlog::info("Version {}, Built: {}", APP_VERSION, BUILD_TIMESTAMP);
  spdlog::info("Author: {}, Email: {}", APP_AUTHOR, APP_CONTACT);
}

/**
 * @brief Sets up the logger with both console and file sinks.
 *
 * This function initializes a logger with two sinks: one for console output
 * with color and another for file output. The console sink logs messages at
 * the info level and above, while the file sink logs messages at the debug
 * level and above. The logger itself is set to log messages at the debug level
 * and above. The logger is then set as the default logger for the application.
 */
void setup_logger() {
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::info);

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logfile.log", true);
  file_sink->set_level(spdlog::level::debug);

  std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
  auto logger = std::make_shared<spdlog::logger>(APP_NAME, sinks.begin(), sinks.end());
  logger->set_level(spdlog::level::debug);
  spdlog::set_default_logger(logger);
}

int main(int argc, char *argv[]) {
  // Setup the global logger
  setup_logger();

  // Parse command line arguments
  std::string filename;
  std::string mode;

  // Command line parameter parsing using CLI11
  CLI::App app{APP_NAME};
  app.set_version_flag("-v,--version", APP_VERSION);
  app.add_option("-m,--mode", mode, "Choose the mode: 'parse' or 'modify'")
      ->default_val("parse")
      ->check(CLI::IsMember({"parse", "modify"}));
  app.add_option("-f,--file", filename, "Specify the file to process")
      ->check(CLI::ExistingFile)
      ->required();
  auto formatter = std::make_shared<CLI::Formatter>();
  formatter->column_width(45);
  app.formatter(formatter);
  CLI11_PARSE(app, argc, argv);

  print_info();

  // Begin reading the file
  LibFile libfile(filename);
  libfile.read();

  if (mode == "parse") {
    libfile.parse();
  } else if (mode == "modify") {
    libfile.modify();
  }

  spdlog::info("All Done!");
  return 0;
}