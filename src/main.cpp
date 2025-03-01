// ./src/main.cpp
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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

// --- New functions for threaded operations ---

void parseLibFile(const std::string &filename) {
  std::string log_filename = filename + ".parse.log";
  setupLogger(log_filename);

  spdlog::info("Starting parse for file: '{}' ...", filename);
  try {
    LibFile libfile(filename);
    libfile.parse();
    libfile.writeJsonToFile(filename + ".json");
    spdlog::info("Successfully parsed file: '{}'", filename);
  } catch (const std::exception &e) {
    spdlog::error("Error parsing file '{}': {}", filename, e.what());
  }
}

void monoCheckLibFile(const std::string &filename, bool is_slew) {
  std::string log_filename = filename + ".mono.log";
  setupLogger(log_filename); // Each thread sets up its own logger instance to file.

  spdlog::info("Starting monotonicity check for file: '{}', input slew: {}", filename, is_slew);
  try {
    LibFile libfile(filename);
    if (!std::filesystem::exists(filename + ".json")) {
      spdlog::info("JSON file not found. Parsing Liberty file first.");
      libfile.parse();
      libfile.writeJsonToFile(filename + ".json");
    }
    libfile.mono(is_slew);
    spdlog::info("Successfully completed monotonicity check for file: '{}'", filename);
  } catch (const std::exception &e) {
    spdlog::error("Error during monotonicity check for file '{}': {}", filename, e.what());
  }
}

int main(int argc, char *argv[]) {
  // Parse command line arguments
  std::vector<std::string> filenames; // Support multiple files
  std::string log_filename;
  bool is_slew = false;

  // Command line parameter parsing using CLI11
  CLI::App app{APP_NAME};
  app.set_version_flag("-v,--version", APP_VERSION);

  auto formatter = std::make_shared<CLI::Formatter>();
  formatter->column_width(40);
  app.formatter(formatter);

  // Add subcommands for parse mode
  CLI::App *parse_cmd = app.add_subcommand("parse", "Parse the Liberty file and write JSON to a file");
  parse_cmd->add_option("-f,--file", filenames, "Specify the lib file to process")
      ->check(CLI::ExistingFile)
      ->required();
  parse_cmd->add_option("-l,--log", log_filename,
                        "Specify the log filename. Default: <input_file>.parse.log");
  parse_cmd->callback([&] {
    printInfo();
    // Sequential parsing
    for (const auto &filename : filenames) {
      parseLibFile(filename);
    }
  });
  parse_cmd->formatter(formatter);

  // Add subcommands for mono check mode
  CLI::App *mono_cmd = app.add_subcommand("mono", "Check the monotonicity of timing arc values");
  mono_cmd->add_option("-f,--file", filenames, "Specify the lib file to process")
      ->check(CLI::ExistingFile)
      ->required();
  mono_cmd->add_option("-l,--log", log_filename, "Specify the log filename. Default: <input_file>.mono.log");
  mono_cmd->add_flag("-s,--slew", is_slew, "Specify that monotonicity checks also include input slew.");
  mono_cmd->callback([&] {
    printInfo();
    // Parallel monotonicity check
    std::vector<std::thread> threads;
    for (const auto &filename : filenames) {
      threads.emplace_back(monoCheckLibFile, filename, is_slew);
    }
    for (auto &thread : threads) {
      thread.join();
    }
  });
  mono_cmd->formatter(formatter);

  CLI11_PARSE(app, argc, argv);

  // End of program
  spdlog::info("All Done!");
  return 0;
}