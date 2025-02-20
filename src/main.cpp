// ./src/main.cpp
#include <filesystem>
#include <iostream>
#include <string>

#include "CLI/CLI.hpp"
#include "si2dr_liberty.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "lib_file.hpp"
#include "version.h"

/**
 * @brief Prints application information including version, build timestamp, author, and contact
 * email.
 *
 * This function uses the spdlog library to log the application's version, build timestamp,
 * author, and contact email to the console or a log file.
 *
 * The following macros are expected to be defined:
 * - APP_VERSION: The version of the application.
 * - BUILD_TIMESTAMP: The timestamp when the application was built.
 * - APP_AUTHOR: The author of the application.
 * - APP_CONTACT: The contact email of the author.
 */
void printInfo() {
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
void setupLogger(const std::string &logger_name) {
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::info);

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logger_name, true);
  file_sink->set_level(spdlog::level::debug);

  std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
  auto logger = std::make_shared<spdlog::logger>(APP_NAME, sinks.begin(), sinks.end());
  logger->set_level(spdlog::level::debug);
  spdlog::set_default_logger(logger);

  spdlog::info("Debug log in: '{}'", logger_name);
}

int main(int argc, char *argv[]) {
  // Parse command line arguments
  std::string filename;
  std::string mode;

  // Command line parameter parsing using CLI11
  CLI::App app{APP_NAME};
  app.set_version_flag("-v,--version", APP_VERSION);
  app.add_option("-m,--mode", mode, "Choose the mode to run the program in")
      ->default_val("parse")
      ->check(CLI::IsMember({"parse", "modify", "mono"}));
  app.add_option("-f,--file", filename, "Specify the file to process")->check(CLI::ExistingFile)->required();
  auto formatter = std::make_shared<CLI::Formatter>();
  formatter->column_width(50);
  app.formatter(formatter);
  CLI11_PARSE(app, argc, argv);

  printInfo();

  // Create a LibFile object and run the specified mode
  LibFile libfile(filename);

  if (mode == "parse") {
    setupLogger(filename + ".parse.log");
    libfile.read();
    libfile.parse();
    libfile.writeJsonToFile(filename + ".json");
  } else if (mode == "modify") {
    libfile.modify();
  } else if (mode == "mono") {
    setupLogger(filename + ".mono.log");
    // If a JSON file exists, read from it directly.
    // Otherwise, parse the Liberty file and write the JSON to a file.
    if (!std::filesystem::exists(filename)) {
      spdlog::info("JSON file not found.");
      libfile.read();
      libfile.parse();
      libfile.writeJsonToFile(filename + ".json");
    }
    libfile.mono();
  }

  spdlog::info("All Done!");
  return 0;
}