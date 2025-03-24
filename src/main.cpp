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
 * @brief Initializes and sets up the global logger with both console and file sinks.
 *
 * This function configures the global logger to output log messages to both the console
 * and a log file. The log file is named after the application name with a ".log" extension.
 * The console sink is set to display messages at the info level, while the file sink
 * captures messages at the debug level. The logger is then set to the info level.
 *
 * The function also logs the application version, build timestamp, author, and contact information.
 *
 * @note The following macros are expected to be defined:
 * - APP_NAME: The name of the application.
 * - APP_VERSION: The version of the application.
 * - BUILD_TIMESTAMP: The build timestamp of the application.
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
  logger->set_level(spdlog::level::debug);
  spdlog::set_default_logger(logger);

  spdlog::info("Version {}, Built: {}", APP_VERSION, BUILD_TIMESTAMP);
  spdlog::info("Author: {}, Email: {}", APP_AUTHOR, APP_CONTACT);
  spdlog::info("Global log file in: '{}'", app_name + ".log");
}

/**
 * @brief Parses a library file and writes the parsed data to a JSON file.
 *
 * This function takes the path to a library file and an optional log file name.
 * It creates a `LibFile` object, logs the start of the parsing process, and
 * attempts to parse the file. If parsing is successful, the parsed data is
 * written to a JSON file. If an error occurs during parsing, an error message
 * is logged.
 *
 * @param library_path The path to the library file to be parsed.
 * @param log_file_name The name of the log file. If empty, a default log file
 *                      name is generated based on the library file name.
 */
void parseLibFile(const std::string &library_path, const std::string log_file_name) {
  std::string logname = log_file_name.empty()
                            ? std::filesystem::path(library_path).stem().string() + ".parse.log"
                            : log_file_name;
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

/**
 * @brief Performs a monotonicity check on a given library file.
 *
 * This function checks the monotonicity of the specified library file and logs the process.
 * It uses the provided log file name or generates one based on the library file name if not provided.
 *
 * @param library_path The path to the library file to be checked.
 * @param log_file_name The name of the log file to be used. If empty, a default log file name is
 * generated.
 * @param is_slew A boolean indicating whether input slew should be considered during the check.
 */
void monoCheckLibFile(const std::string &library_path, const std::string log_file_name, bool is_slew) {
  std::string logname = log_file_name.empty()
                            ? std::filesystem::path(library_path).stem().string() + ".mono.log"
                            : log_file_name;
  LibFile libfile(library_path, logname);

  libfile.logger_->info("Starting monotonicity check for file: '{}', input slew: {}", library_path,
                        is_slew);

  try {
    libfile.mono(is_slew);
    libfile.logger_->info("Successfully completed monotonicity check for file: '{}'", library_path);
  } catch (const std::exception &e) {
    libfile.logger_->error("Error during monotonicity check for file '{}': {}", library_path, e.what());
  }
}

/**
 * @brief Generates supercells for a given library file and logs the process.
 *
 * This function takes a library file path, a log file name, and a chain length as input.
 * It generates supercells for the specified library file and logs the process, including
 * any errors encountered during the generation.
 *
 * @param library_path The path to the library file for which supercells are to be generated.
 * @param log_file_name The name of the log file where the process will be logged. If empty,
 *                      a default log file name will be generated based on the library file name.
 * @param chain_length The length of the chain for supercell generation. Must be greater than or equal
 * to 1.
 */
void supercellLibFile(const std::string &library_path, const std::string &log_file_name,
                      int chain_length) {
  std::string logname = log_file_name.empty()
                            ? std::filesystem::path(library_path).stem().string() + ".supercell.log"
                            : log_file_name;
  LibFile libfile(library_path, logname);

  // Check chain length validity
  if (chain_length < 1) {
    libfile.logger_->error("Invalid chain length: {}. Chain length must be >= 1.", chain_length);
    return;
  }

  libfile.logger_->info("Starting supercell generation for file: '{}', chain length: {}", library_path,
                        chain_length);
  try {
    libfile.supercell(chain_length);
    libfile.logger_->info("Successfully generated supercells for file: '{}'", library_path);
  } catch (const std::exception &e) {
    libfile.logger_->error("Error during supercell generation for file '{}': {}", library_path,
                           e.what());
  }
}

void compareLibFiles(const std::string &ref_lib, const std::string &comp_lib, const double reltol,
                     const double abstol, std::string &report_file_name) {
  std::string ref_logname = std::filesystem::path(ref_lib).stem().string() + ".cmp.log";
  std::string comp_logname = std::filesystem::path(comp_lib).stem().string() + ".cmp.log";
  LibFile ref_libfile(ref_lib, ref_logname), comp_libfile(comp_lib, comp_logname);

  // Check relative tolerance validity
  if (reltol < 0.0) {
    spdlog::error("Invalid relative tolerance: {}. Relative tolerance must be >= 0.0.", reltol);
    return;
  }
  if (report_file_name.empty()) {
    report_file_name = comp_libfile.basename_ + ".cmp.md";
  } else if (std::filesystem::path(report_file_name).extension() != ".txt" &&
             std::filesystem::path(report_file_name).extension() != ".md") {
    // report file name not end in .txt or .md, warn user and append .md
    report_file_name = report_file_name + ".md";
    spdlog::warn("Report file name does not end in .txt or .md. Report will be written to '{}'",
                 report_file_name);
  }

  // TODO: Compare the two JSON files
  LibraryComparator comparator(ref_libfile, comp_libfile, reltol, abstol);
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
        parseLibFile(library_path, log_file_name = "");
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
  mono_cmd->add_option("-l,--log", log_file_name,
                       "Specify the log file name. Default: <basename>.mono.log");
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
          parseLibFile(library_path, log_file_name = "");
        }
      }
      spdlog::info("All JSON files prepared.");
      // Parallel monotonicity check
      std::vector<std::thread> threads;
      for (const auto &library_path : library_paths) {
        threads.emplace_back(monoCheckLibFile, library_path, log_file_name = "", is_slew);
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
  CLI::App *compare_cmd = app.add_subcommand(
      "compare", "Compare the comparison library against the reference library and report differences");
  compare_cmd->add_option("--ref", ref_lib, "Specify the reference library file")
      ->check(CLI::ExistingFile)
      ->required();
  compare_cmd->add_option("--comp", comp_lib, "Specify the comparison library file")
      ->check(CLI::ExistingFile)
      ->required();
  double abstol = 0.002;
  compare_cmd->add_option("--abstol", abstol,
                          "Specify the absolute tolerance for comparison. Default: 0.002ns");
  double reltol = 0.02;
  compare_cmd->add_option("--reltol", reltol,
                          "Specify the relative tolerance for comparison. Default: 0.02/2.0%");
  compare_cmd->add_option("--report", report_file_name,
                          "Specify the report file name. Default: <comp_lib>.cmp.md");
  compare_cmd->callback([&] {
    printInfo();
    compareLibFiles(ref_lib, comp_lib, reltol, abstol, report_file_name);
  });

  // Add subcommand for supercell generation
  int chain_length = 1;
  CLI::App *supercell_cmd =
      app.add_subcommand("supercell", "Generate supercells for the given Liberty file");
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
          parseLibFile(library_path, log_file_name = "");
        }
      }
      spdlog::info("All JSON files prepared.");
      // Parallel supercell generation
      std::vector<std::thread> threads;
      for (const auto &library_path : library_paths) {
        threads.emplace_back(supercellLibFile, library_path, log_file_name = "", chain_length);
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
  CLI::App *clear_cmd =
      app.add_subcommand("clear", "Clear the log, JSON, map, markdown files in this directory");
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

  // Add subcommand for Verilog generation
  CLI::App *verilog_cmd = app.add_subcommand("verilog", "Generate Verilog file for given Liberty file");
  verilog_cmd->add_option("library_path", library_paths, "Specify the library file to process")
      ->check(CLI::ExistingFile)
      ->required();
  verilog_cmd->add_option("-l,--log", log_file_name,
                          "Specify the log file name. Default: <basename>.verilog.log");
  std::vector<std::string> cell_names = {"AN2D0", "DFQD1"}; // "CMPE42D1"
  verilog_cmd->add_option("--cells", cell_names, "Specify the cell names to generate Verilog for");
  verilog_cmd->callback([&] {
    printInfo();
    // Check if multi files
    if (library_paths.size() > 1) {
      spdlog::info("Running multi-threaded Verilog generation for {} files.", library_paths.size());
      spdlog::info("Each library will write to its own log file.");

      // Sequential json file check
      for (const auto &library_path : library_paths) {
        if (!std::filesystem::exists(std::filesystem::path(library_path).stem().string() + ".json")) {
          spdlog::info("JSON file not found for '{}'. Parsing Liberty file first.", library_path);
          parseLibFile(library_path, log_file_name = "");
        }
      }
      spdlog::info("All JSON files prepared.");
      // Parallel Verilog generation
      std::vector<std::thread> threads;
      for (const auto &library_path : library_paths) {
        // threads.emplace_back(verilogLibFile, library_path, log_file_name = "", cell_names);
      }
      for (auto &thread : threads) {
        thread.join();
      }
      spdlog::info("All threads completed.");
    } else {
      // verilogLibFile(library_paths[0], log_file_name, cell_names);
    }
  });

  // Add subcommand for SPICE generation
  CLI::App *spice_cmd = app.add_subcommand("spice", "Generate SPICE file for given Liberty file");
  spice_cmd->add_option("library_path", library_paths, "Specify the library file to process")
      ->check(CLI::ExistingFile)
      ->required();
  spice_cmd->add_option("-l,--log", log_file_name,
                        "Specify the log file name. Default: <basename>.spice.log");
  spice_cmd->add_option("--cells", cell_names, "Specify the cell names to generate SPICE for");
  spice_cmd->callback([&] {
    printInfo();
    // Check if multi files
    if (library_paths.size() > 1) {
      spdlog::info("Running multi-threaded SPICE generation for {} files.", library_paths.size());
      spdlog::info("Each library will write to its own log file.");

      // Sequential json file check
      for (const auto &library_path : library_paths) {
        if (!std::filesystem::exists(std::filesystem::path(library_path).stem().string() + ".json")) {
          spdlog::info("JSON file not found for '{}'. Parsing Liberty file first.", library_path);
          parseLibFile(library_path, log_file_name = "");
        }
      }
      spdlog::info("All JSON files prepared.");
      // Parallel SPICE generation
      std::vector<std::thread> threads;
      for (const auto &library_path : library_paths) {
        // threads.emplace_back(spiceLibFile, library_path, log_file_name = "", cell_names);
      }
      for (auto &thread : threads) {
        thread.join();
      }
      spdlog::info("All threads completed.");
    } else {
      // spiceLibFile(library_paths[0], log_file_name, cell_names);
    }
  });

  // Add subcommand for funtional equivalence check
  CLI::App *func_cmd = app.add_subcommand("func", "Check functional equivalence of two Liberty files or Verilog files");
  std::string ref_file, comp_file;
  func_cmd->add_option("--ref", ref_file, "Specify the reference Liberty or Verilog file")
      ->check(CLI::ExistingFile)
      ->required();
  func_cmd->add_option("--comp", comp_file, "Specify the comparison Liberty or Verilog file")
      ->check(CLI::ExistingFile)
      ->required();
  func_cmd->add_option("--cells", cell_names, "Specify the cell names to check functional equivalence for");
  func_cmd->callback([&] {
    printInfo();
     // funcLibFile();
  });

  CLI11_PARSE(app, argc, argv);

  // End of program
  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  
  auto now = std::chrono::system_clock::now();
  auto time_now = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_now), "%c");

  spdlog::info("ZlibValidation exited on '{}' at {}", hostname, ss.str());
  return 0;
}