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

void parseLibFile(const std::string &library_file_name, std::string &log_file_name) {
  std::filesystem::path library_file_path(library_file_name);
  std::string filename = library_file_path.stem().string();

  log_file_name = log_file_name.empty() ? filename + ".parse.log" : log_file_name;
  setupLogger(log_file_name);

  spdlog::info("Starting parse for file: '{}' ...", library_file_name);
  try {
    LibFile libfile(library_file_name);
    libfile.parse();
    libfile.writeJsonToFile(filename + ".json");
    spdlog::info("Successfully parsed file: '{}'", library_file_name);
  } catch (const std::exception &e) {
    spdlog::error("Error parsing file '{}': {}", library_file_name, e.what());
  }
}

void monoCheckLibFile(const std::string &library_file_name, std::string &log_file_name, bool is_slew) {
  std::filesystem::path library_file_path(library_file_name);
  std::string filename = library_file_path.stem().string();

  log_file_name = log_file_name.empty() ? filename + ".mono.log" : log_file_name;
  setupLogger(log_file_name); // Each thread sets up its own logger instance to file.

  spdlog::info("Starting monotonicity check for file: '{}', input slew: {}", library_file_name, is_slew);
  try {
    LibFile libfile(library_file_name);
    std::string json_file_name = filename + ".json";
    if (!std::filesystem::exists(json_file_name)) {
      spdlog::info("JSON file not found. Parsing Liberty file first.");
      libfile.parse();
      libfile.writeJsonToFile(json_file_name);
    }
    libfile.mono(json_file_name, is_slew);
    spdlog::info("Successfully completed monotonicity check for file: '{}'", library_file_name);
  } catch (const std::exception &e) {
    spdlog::error("Error during monotonicity check for file '{}': {}", library_file_name, e.what());
  }
}

void supercellLibFile(const std::string &library_file_name, std::string &log_file_name, int chain_length) {
  std::filesystem::path library_file_path(library_file_name);
  std::string filename = library_file_path.stem().string();

  log_file_name = log_file_name.empty() ? filename + ".supercell.log" : log_file_name;
  setupLogger(log_file_name);

  spdlog::info("Starting supercell generation for file: '{}', chain length: {}", library_file_name, chain_length);
  try {
    LibFile libfile(library_file_name);
    std::string json_file_name = filename + ".json";
    if (!std::filesystem::exists(json_file_name)) {
      spdlog::info("JSON file not found. Parsing Liberty file first.");
      libfile.parse();
      libfile.writeJsonToFile(json_file_name);
    }
    libfile.supercell(json_file_name, chain_length);
    spdlog::info("Successfully generated supercells for file: '{}'", library_file_name);
  } catch (const std::exception &e) {
    spdlog::error("Error during supercell generation for file '{}': {}", library_file_name, e.what());
  }
}

int main(int argc, char *argv[]) {
  // Parse command line arguments
  std::vector<std::string> library_file_names; // Support multiple files
  std::string log_file_name = "";
  bool is_slew = false;

  // Command line parameter parsing using CLI11
  CLI::App app{APP_NAME};
  app.set_version_flag("-v,--version", APP_VERSION);

  auto formatter = std::make_shared<CLI::Formatter>();
  formatter->column_width(45);
  app.formatter(formatter);

  // Add subcommand for parse mode
  CLI::App *parse_cmd = app.add_subcommand("parse", "Parse the Liberty file and write JSON to a file");
  parse_cmd->add_option("library_file_name", library_file_names, "Specify the library file to process")
      ->check(CLI::ExistingFile)
      ->required();
  parse_cmd->add_option("-l,--log", log_file_name,
                        "Specify the log file name. Default: <input_file>.parse.log");
  parse_cmd->callback([&] {
    printInfo();
    // Sequential parsing
    for (const auto &library_file_name : library_file_names) {
      parseLibFile(library_file_name, log_file_name);
    }
  });
  parse_cmd->formatter(formatter);

  // Add subcommand for mono check mode
  CLI::App *mono_cmd = app.add_subcommand("mono", "Check the monotonicity of timing arc values");
  mono_cmd->add_option("library_file_name", library_file_names, "Specify the library file to process")
      ->check(CLI::ExistingFile)
      ->required();
  mono_cmd->add_option("-l,--log", log_file_name, "Specify the log file name. Default: <input_file>.mono.log");
  mono_cmd->add_flag("-s,--slew", is_slew, "Specify that monotonicity checks also include input slew.");
  mono_cmd->callback([&] {
    printInfo();
    // // Parallel monotonicity check
    // std::vector<std::thread> threads;
    // for (const auto &library_file_name : library_file_names) {
    //   threads.emplace_back(monoCheckLibFile, library_file_name, log_file_name, is_slew);
    // }
    // for (auto &thread : threads) {
    //   thread.join();
    // }
    for (const auto &library_file_name : library_file_names) {
      monoCheckLibFile(library_file_name, log_file_name, is_slew);
    }
  });
  mono_cmd->formatter(formatter);

  // Add subcommand for compare mode
  std::string ref_lib, comp_lib;
  double retol = 0.01;
  CLI::App *compare_cmd = app.add_subcommand("compare", "Compare the comparison library against the reference library and report differences");
  compare_cmd->add_option("--ref", ref_lib, "Specify the reference library file")
      ->check(CLI::ExistingFile)
      ->required();
  compare_cmd->add_option("--comp", comp_lib, "Specify the comparison library file")
    ->check(CLI::ExistingFile)
    ->required();
  compare_cmd->add_option("-l,--log", log_file_name, "Specify the log file name. Default: <comp_lib>.compare.log");
  compare_cmd->add_option("--reltol", retol, "Specify the relative tolerance for comparison. Default: 0.01");
  compare_cmd->callback([&] {
    printInfo();
    // Compare the two Liberty files
    std::filesystem::path comp_path(comp_lib);
    std::string comp_filename = comp_path.stem().string();
    log_file_name = log_file_name.empty() ? comp_filename + ".compare.log" : log_file_name;
    parseLibFile(ref_lib, log_file_name);
    parseLibFile(comp_lib, log_file_name);
    // TODO: Compare the two JSON files
  });
  compare_cmd->formatter(formatter);

  // Add subcommand for supercell generation
  int chain_length = 1;
  CLI::App *supercell_cmd = app.add_subcommand("supercell", "Generate supercells for the given Liberty file");
  supercell_cmd->add_option("library_file_name", library_file_names, "Specify the library file to process")
      ->check(CLI::ExistingFile)
      ->required();
  supercell_cmd->add_option("-l,--log", log_file_name, "Specify the log file name. Default: <input_file>.supercell.log");
  supercell_cmd->add_option("-c,--chain", chain_length, "Specify the chain length for supercell generation. Default: 1");
  supercell_cmd->callback([&] {
    printInfo();
    // Sequential supercell generation
    for (const auto &library_file_name : library_file_names) {
      supercellLibFile(library_file_name, log_file_name, chain_length);
    }
  });
  supercell_cmd->formatter(formatter);

  // Add subcommand for zlibboost
  CLI::App *zlibboost_cmd = app.add_subcommand("zlibboost", "ZlibBoost - Multi-threaded Library Processing Tool");
  std::string config_dir = "/home/songzx/Projects/zlibboost/config.tcl";
  std::string python_dir = "/home/guocj/anaconda3/envs/myenv/bin/python";
  std::string main_py_dir = "/home/songzx/Projects/zlibboost/zlibboost.py";
  zlibboost_cmd->add_option("-c, --config", config_dir, "Specify the configuration TCL file. Default: /home/songzx/Projects/zlibboost/config.tcl");
  zlibboost_cmd->add_option("--python", python_dir, "Specify the python directory. Default: /home/guocj/anaconda3/envs/myenv/bin/python");
  zlibboost_cmd->add_option("--main", main_py_dir, "Specify the main python script directory. Default: /home/songzx/Projects/zlibboost/zlibboost.py");
  zlibboost_cmd->callback([&] {
    printInfo();
    // Run the ZlibBoost tool
    std::string command = python_dir + " " + main_py_dir + " -c " + config_dir;
    spdlog::info("Running ZlibBoost with command: '{}'", command);
    int ret = std::system(command.c_str());
    if (ret == 0) {
      spdlog::info("ZlibBoost completed successfully.");
    } else {
      spdlog::error("ZlibBoost failed with return code: {}", ret);
    }
  });
  zlibboost_cmd->formatter(formatter);

  // Add subcommand for clear
  CLI::App *clear_cmd = app.add_subcommand("clear", "Clear the log files and JSON files in this directory");
  clear_cmd->callback([&] {
    printInfo();
    // Clear log files and JSON files
    std::filesystem::path current_dir = std::filesystem::current_path();
    for (const auto &entry : std::filesystem::directory_iterator(current_dir)) {
      if (entry.path().extension() == ".log" || entry.path().extension() == ".json") {
        spdlog::info("Removing file: '{}'", entry.path().string());
        std::filesystem::remove(entry.path());
      }
    }
    spdlog::info("All log files and JSON files cleared.");
  });

  CLI11_PARSE(app, argc, argv);

  // End of program
  spdlog::info("All Done!");
  return 0;
}