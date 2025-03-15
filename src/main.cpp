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

#include "compare.hpp"
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
  // Set Global Logger
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::info);
  std::string app_name = APP_NAME;
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(app_name + ".log", true);
  file_sink->set_level(spdlog::level::debug);

  std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
  auto logger = std::make_shared<spdlog::logger>(APP_NAME, sinks.begin(), sinks.end());
  logger->set_level(spdlog::level::info);
  spdlog::set_default_logger(logger);

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

void parseLibFile(const std::string &library_path, const std::string log_file_name) {
  std::string logname = log_file_name.empty() ? std::filesystem::path(library_path).stem().string() + ".parse.log" : log_file_name;
  LibFile libfile(library_path, logname);

  libfile.logger_->info("Starting parse for file: '{}' ...", library_path);

  try {
    libfile.parse();
    libfile.writeJsonToFile();
    libfile.logger_->info("Successfully parsed file: '{}'", library_path);
  } catch (const std::exception &e) {
    libfile.logger_->error("Error parsing file '{}': {}", library_path, e.what());
  }
}

void monoCheckLibFile(const std::string &library_path, const std::string log_file_name, bool is_slew) {
  std::string logname = log_file_name.empty() ? std::filesystem::path(library_path).stem().string() + ".mono.log" : log_file_name;
  LibFile libfile(library_path, logname);

  libfile.logger_->info("Starting monotonicity check for file: '{}', input slew: {}", library_path, is_slew);

  try {
    libfile.mono(is_slew);
    libfile.logger_->info("Successfully completed monotonicity check for file: '{}'", library_path);
  } catch (const std::exception &e) {
    libfile.logger_->error("Error during monotonicity check for file '{}': {}", library_path, e.what());
  }
}

void supercellLibFile(const std::string &library_path, const std::string &log_file_name, int chain_length) {
  std::string logname = log_file_name.empty() ? std::filesystem::path(library_path).stem().string() + ".supercell.log" : log_file_name;
  LibFile libfile(library_path, logname);

  // Check chain length validity
  if (chain_length < 1) {
    libfile.logger_->error("Invalid chain length: {}. Chain length must be >= 1.", chain_length);
    return;
  }

  libfile.logger_->info("Starting supercell generation for file: '{}', chain length: {}", library_path, chain_length);
  try {
    libfile.supercell(chain_length);
    libfile.logger_->info("Successfully generated supercells for file: '{}'", library_path);
  } catch (const std::exception &e) {
    libfile.logger_->error("Error during supercell generation for file '{}': {}", library_path, e.what());
  }
}

void compareLibFiles(const std::string &ref_lib, const std::string &comp_lib,
                     const double reltol, std::string &report_file_name) {
  std::string ref_logname = std::filesystem::path(ref_lib).stem().string() + ".cmp.log";
  std::string comp_logname = std::filesystem::path(comp_lib).stem().string() + ".cmp.log";
  LibFile ref_libfile(ref_lib, ref_logname), comp_libfile(comp_lib, comp_logname);

  // Check relative tolerance validity
  if (reltol < 0.0) {
    spdlog::error("Invalid relative tolerance: {}. Relative tolerance must be >= 0.0.", reltol);
    return;
  }

  // TODO: Compare the two JSON files
  LibraryComparator comparator(ref_libfile, comp_libfile, reltol);
  report_file_name = report_file_name.empty() ? comp_libfile.basename_ + ".cmp.txt" : report_file_name;
  comparator.generateReport(report_file_name);
  spdlog::info("Comparison completed. Report written to: '{}'", report_file_name);
}

int main(int argc, char *argv[]) {
  // Parse command line arguments
  std::vector<std::string> library_paths; // Support multiple files
  std::string log_file_name = "";

  // Command line parameter parsing using CLI11
  CLI::App app{APP_NAME};
  app.set_version_flag("-v,--version", APP_VERSION);

  // Add subcommand for parse mode
  CLI::App *parse_cmd = app.add_subcommand("parse", "Parse the Liberty file and write JSON to a file");
  parse_cmd->add_option("library_path", library_paths, "Specify the library file to process")
      ->check(CLI::ExistingFile)
      ->required();
  parse_cmd->add_option("-l,--log", log_file_name,
                        "Specify the log file name. Default: <basename>.parse.log");
  parse_cmd->callback([&] {
    printInfo();
    // Check if multi files
    if (library_paths.size() > 1) {
      spdlog::info("Running sequential parsing for {} files.", library_paths.size());
      spdlog::info("Each library will write to its own log file.");
      // Sequential parsing
      for (const auto &library_path : library_paths) {
        parseLibFile(library_path, log_file_name="");
      }
    } else {
      parseLibFile(library_paths[0], log_file_name);
    }
  });

  // Add subcommand for mono check mode
  bool is_slew = false;
  CLI::App *mono_cmd = app.add_subcommand("mono", "Check the monotonicity of timing arc values");
  mono_cmd->add_option("library_path", library_paths, "Specify the library file to process")
      ->check(CLI::ExistingFile)
      ->required();
  mono_cmd->add_option("-l,--log", log_file_name, "Specify the log file name. Default: <basename>.mono.log");
  mono_cmd->add_flag("-s,--slew", is_slew, "Specify that monotonicity checks also include input slew.");
  mono_cmd->callback([&] {
    printInfo();
    // Check if multi files
    if (library_paths.size() > 1) {
      spdlog::info("Running multi-threaded monotonicity check for {} files.", library_paths.size());
      spdlog::info("Each thread will write to its own log file.");
      
      // Sequential json file check
      for (const auto &library_path : library_paths) {
        if (!std::filesystem::exists(std::filesystem::path(library_path).stem().string() + ".json")) {
          spdlog::info("JSON file not found for '{}'. Parsing Liberty file first.", library_path);
          parseLibFile(library_path, log_file_name="");
        }
      }
      spdlog::info("All JSON files prepared.");
      // Parallel monotonicity check
      std::vector<std::thread> threads;
      for (const auto &library_path : library_paths) {
        threads.emplace_back(monoCheckLibFile, library_path, log_file_name="", is_slew);
      }
      for (auto &thread : threads) {
        thread.join();
      }
      spdlog::info("All threads completed.");
    } else {
      monoCheckLibFile(library_paths[0], log_file_name, is_slew);
    }
  });

  // Add subcommand for compare mode
  std::string ref_lib, comp_lib, report_file_name;
  // ref_lib = "/home/songzx/Projects/ZlibValidation/test/tcbn65lpbc.nldm.pt.lib";
  // comp_lib = "/home/songzx/Projects/ZlibValidation/test/tcbn65lpbc.ski.lib";
  double retol = 0.01;
  CLI::App *compare_cmd = app.add_subcommand(
      "compare", "Compare the comparison library against the reference library and report differences");
  compare_cmd->add_option("--ref", ref_lib, "Specify the reference library file")
      ->check(CLI::ExistingFile)
      ->required();
  compare_cmd->add_option("--comp", comp_lib, "Specify the comparison library file")
      ->check(CLI::ExistingFile)
      ->required();
  // compare_cmd->add_option("-l,--log", log_file_name,
  //                         "Specify the log file name. Default: <comp_lib>.cmp.log");
  compare_cmd->add_option("--reltol", retol, "Specify the relative tolerance for comparison. Default: 0.01");
  compare_cmd->add_option("--report", report_file_name,
                          "Specify the report file name. Default: <comp_lib>.cmp.txt");
  compare_cmd->callback([&] {
    printInfo();
    compareLibFiles(ref_lib, comp_lib, retol, report_file_name);
  });

  // Add subcommand for supercell generation
  int chain_length = 1;
  CLI::App *supercell_cmd = app.add_subcommand("supercell", "Generate supercells for the given Liberty file");
  supercell_cmd->add_option("library_path", library_paths, "Specify the library file to process")
      ->check(CLI::ExistingFile)
      ->required();
  supercell_cmd->add_option("-l,--log", log_file_name,
                            "Specify the log file name. Default: <basename>.supercell.log");
  supercell_cmd->add_option("-c,--chain", chain_length,
                            "Specify the chain length for supercell generation. Default: 1");
  supercell_cmd->callback([&] {
    printInfo();
    // Check if multi files
    if (library_paths.size() > 1) {
      spdlog::info("Running multi-threaded supercell generation for {} files.", library_paths.size());
      spdlog::info("Each library will write to its own log file.");

      // Sequential json file check
      for (const auto &library_path : library_paths) {
        if (!std::filesystem::exists(std::filesystem::path(library_path).stem().string() + ".json")) {
          spdlog::info("JSON file not found for '{}'. Parsing Liberty file first.", library_path);
          parseLibFile(library_path, log_file_name="");
        }
      }
      spdlog::info("All JSON files prepared.");
      // Parallel supercell generation
      std::vector<std::thread> threads;
      for (const auto &library_path : library_paths) {
        threads.emplace_back(supercellLibFile, library_path, log_file_name="", chain_length);
      }
      for (auto &thread : threads) {
        thread.join();
      }
      spdlog::info("All threads completed.");
    } else {
      supercellLibFile(library_paths[0], log_file_name, chain_length);
    }
  });

  // Add subcommand for zlibboost
  CLI::App *zlibboost_cmd =
      app.add_subcommand("zlibboost", "ZlibBoost - Multi-threaded Library Processing Tool");
  std::string config_dir = "/home/songzx/Projects/zlibboost/config.tcl";
  std::string python_dir = "/home/guocj/anaconda3/envs/myenv/bin/python";
  std::string main_py_dir = "/home/songzx/Projects/zlibboost/zlibboost.py";
  zlibboost_cmd->add_option(
      "-c, --config", config_dir,
      "Specify the configuration TCL file. Default: /home/songzx/Projects/zlibboost/config.tcl");
  zlibboost_cmd->add_option(
      "--python", python_dir,
      "Specify the python directory. Default: /home/guocj/anaconda3/envs/myenv/bin/python");
  zlibboost_cmd->add_option(
      "--main", main_py_dir,
      "Specify the main python script directory. Default: /home/songzx/Projects/zlibboost/zlibboost.py");
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

  // Add subcommand for clear
  CLI::App *clear_cmd = app.add_subcommand("clear", "Clear the log, JSON, map, markdown files in this directory");
  clear_cmd->callback([&] {
    printInfo();
    std::filesystem::path current_dir = std::filesystem::current_path();
    for (const auto &entry : std::filesystem::directory_iterator(current_dir)) {
      if (entry.path().extension() == ".log" || entry.path().extension() == ".json" ||
          entry.path().extension() == ".map" || entry.path().extension() == ".md") {
        spdlog::info("Removing file: '{}'", entry.path().string());
        std::filesystem::remove(entry.path());
      }
    }
    spdlog::info("All log, JSON, map, markdown files cleared.");
  });

  CLI11_PARSE(app, argc, argv);

  // End of program
  spdlog::info("All Done!");
  return 0;
}