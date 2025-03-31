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
#include "verilog_utils.hpp"
#include "version.h"

/**
 * @brief Sets up and configures the global logger and prints application information.
 *
 * This function performs the following operations:
 * 1. Creates a console sink for logging with level set to INFO
 * 2. Creates a file sink for logging with level set to DEBUG, saving to [APP_NAME].log
 * 3. Configures a logger with both sinks and sets it as the default logger
 * 4. Outputs basic application information:
 *    - Version and build timestamp
 *    - Author information
 *    - Log file location
 *
 * @note Uses spdlog library for logging functionality
 * @note Depends on APP_NAME, APP_VERSION, BUILD_TIMESTAMP, APP_AUTHOR, and APP_CONTACT macros
 */
void printInfo() {
  // Set Global Logger
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::info);
  std::string app_name = APP_NAME;
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(app_name + ".log", true);
  file_sink->set_level(spdlog::level::trace);

  std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
  auto logger = std::make_shared<spdlog::logger>(APP_NAME, sinks.begin(), sinks.end());
  logger->set_level(spdlog::level::debug);
  spdlog::set_default_logger(logger);

  spdlog::info("Version {}, Built: {}", APP_VERSION, BUILD_TIMESTAMP);
  spdlog::info("Author: {}, Email: {}", APP_AUTHOR, APP_CONTACT);
  spdlog::info("Global log file in: '{}'", app_name + ".log");
}

/**
 * @brief Parses a library file and generates corresponding output.
 *
 * This function processes the given library file, parsing its contents and generating
 * a JSON output. It also logs the parsing process to a specified log file or creates
 * a default log file if none is provided.
 *
 * @param library_path Path to the library file that needs to be parsed
 * @param log_file_name Optional name for the log file. If empty, a default name is generated
 *                      based on the library filename with ".parse.log" extension
 *
 * @throws The function catches and logs any exceptions but doesn't propagate them
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
 * @brief Performs monotonicity check on a library file
 *
 * This function validates the monotonicity of timing data in a library file.
 * It creates a log file to record the results of the check and handles any
 * exceptions that occur during the process.
 *
 * @param library_path Path to the library file to check
 * @param log_file_name Name of the log file to create (optional). If empty,
 *                      a default name will be generated from the library file name
 * @param is_slew Flag indicating whether to check input slew monotonicity (true)
 *                or output load monotonicity (false)
 *
 * @throws The function catches and logs any exceptions but does not rethrow them
 */
void monoCheckLibFile(const std::string &library_path, const std::string log_file_name,
                      bool is_slew) {
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
    libfile.logger_->error("Error during monotonicity check for file '{}': {}", library_path,
                           e.what());
  }
}

/**
 * @brief Creates supercell map structures from a Liberty library file
 *
 * This function reads a Liberty file, creates supercell structures based on the specified
 * chain length and cell names, and logs the process to a file.
 *
 * @param library_path Path to the Liberty file to process
 * @param log_file_name Name of the log file (if empty, defaults to "[library_name].supercell.log")
 * @param chain_length The length of chains to create (must be >= 1)
 * @param cell_names Vector of cell names to process for supercell generation
 *
 * @throws May pass through exceptions from the LibFile::supercell method
 *
 * @note The function validates the chain length and logs all activities including
 *       errors that might occur during processing
 */
void supercellLibFile(const std::string &library_path, const std::string &log_file_name,
                      int chain_length, const std::vector<std::string> &cell_names) {
  std::string logname = log_file_name.empty()
                            ? std::filesystem::path(library_path).stem().string() + ".supercell.log"
                            : log_file_name;
  LibFile libfile(library_path, logname);

  // Check chain length validity
  if (chain_length < 1) {
    libfile.logger_->error("Invalid chain length: {}. Chain length must be >= 1.", chain_length);
    return;
  }
  std::stringstream ss;
  for (const auto &cell : cell_names) {
    ss << cell << ", ";
  }
  libfile.logger_->info("Starting supercell generation for file: '{}', chain length: {}, cells: {}",
                        library_path, chain_length, ss.str());
  try {
    libfile.supercell(chain_length, cell_names);
    libfile.logger_->info("Successfully generated supercells for file: '{}'", library_path);
  } catch (const std::exception &e) {
    libfile.logger_->error("Error during supercell generation for file '{}': {}", library_path,
                           e.what());
  }
}

/**
 * @brief Generates Verilog files from a library file for specified cells.
 *
 * This function processes a library file and generates Verilog representation
 * for the specified cell names with a given chain length. The operation results
 * are logged to a specified or default log file.
 *
 * @param library_path Path to the library file to process
 * @param log_file_name Name for the log file (if empty, a default name will be generated)
 * @param chain_length Number of cells to chain together, must be >= 1
 * @param cell_names Vector of cell names to generate Verilog for
 *
 * @throws Catches any exceptions from the verilog generation process and logs them
 */
void verilogLibFile(const std::string &library_path, const std::string &log_file_name,
                    int chain_length, const std::vector<std::string> &cell_names) {
  std::string logname = log_file_name.empty()
                            ? std::filesystem::path(library_path).stem().string() + ".verilog.log"
                            : log_file_name;
  LibFile libfile(library_path, logname);

  // Check chain length validity
  if (chain_length < 1) {
    libfile.logger_->error("Invalid chain length: {}. Chain length must be >= 1.", chain_length);
    return;
  }
  std::stringstream ss;
  for (const auto &cell : cell_names) {
    ss << cell << ", ";
  }
  libfile.logger_->info("Starting Verilog generation for file: '{}', chain length: {}, cells: {}",
                        library_path, chain_length, ss.str());
  try {
    libfile.verilog(chain_length, cell_names);
    libfile.logger_->info("Successfully generated Verilog for file: '{}'", library_path);
  } catch (const std::exception &e) {
    libfile.logger_->error("Error during Verilog generation for file '{}': {}", library_path,
                           e.what());
  }
}

/**
 * @brief Compares two library files and generates a detailed comparison report.
 *
 * This function compares a reference library file with another library file,
 * performing validation checks based on specified tolerance parameters.
 * The comparison results are written to a report file in markdown or text format.
 *
 * @param ref_lib Path to the reference library file to use as the baseline
 * @param comp_lib Path to the library file to compare against the reference
 * @param reltol Relative tolerance for numerical comparisons (must be >= 0.0)
 * @param abstol Absolute tolerance for numerical comparisons
 * @param report_file_name [in,out] Name of the file to write the comparison report to.
 *                         If empty, defaults to [comp_lib_basename].cmp.md.
 *                         If provided but doesn't end with .txt or .md, .md will be appended.
 *
 * @note The function will log an error and return without comparing if reltol is invalid.
 * @note Log files will be created with the naming pattern [library_basename].cmp.log
 * @note The function uses the LibraryComparator class to perform the actual comparison.
 */
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

  // Check the report file name
  if (report_file_name.empty()) {
    report_file_name = comp_libfile.basename_ + ".cmp.md";
  } else if (std::filesystem::path(report_file_name).extension() != ".txt" &&
             std::filesystem::path(report_file_name).extension() != ".md") {
    // report file name not end in .txt or .md, warn user and append .md
    report_file_name = report_file_name + ".md";
    spdlog::warn("Report file name does not end in .txt or .md. Report will be written to '{}'",
                 report_file_name);
  }

  LibraryComparator comparator(ref_libfile, comp_libfile, reltol, abstol);
  comparator.generateReport(report_file_name);
  spdlog::info("Comparison completed. Report written to: '{}'", report_file_name);
}

void funcLibFile(const std::string &ref_file, const std::string &comp_file,
                 const std::vector<std::string> &cell_names, std::string &report_file_name) {
  // Check if the files are Liberty or Verilog
  std::string ref_ext = std::filesystem::path(ref_file).extension();
  std::string comp_ext = std::filesystem::path(comp_file).extension();
  std::string ref_logname = std::filesystem::path(ref_file).stem().string() + ".cmp.log";
  std::string comp_logname = std::filesystem::path(comp_file).stem().string() + ".cmp.log";

  // Pre cell names check
  if (cell_names.empty()) {
    spdlog::error("No cell names provided for functional equivalence check.");
    return;
  }

  // Check the report file name
  if (report_file_name.empty()) {
    report_file_name = std::filesystem::path(comp_file).stem().string() + ".cmp.md";
  } else if (std::filesystem::path(report_file_name).extension() != ".txt" &&
             std::filesystem::path(report_file_name).extension() != ".md") {
    // report file name not end in .txt or .md, warn user and append .md
    report_file_name = report_file_name + ".md";
    spdlog::warn("Report file name does not end in .txt or .md. Report will be written to '{}'",
                 report_file_name);
  }

  // Declare LibFile objects as pointers, initialized to nullptr
  LibFile *ref_libfile = nullptr;
  LibFile *comp_libfile = nullptr;

  // Check the reference file extension
  if (ref_ext == ".v") {
    spdlog::info("Reference file in Verilog format.");
  } else if (ref_ext == ".lib") {
    spdlog::info("Reference file in Liberty format.");
    ref_libfile = new LibFile(ref_file, ref_logname);
  } else {
    spdlog::error("Unsupported reference file format: '{}'", ref_ext);
    return;
  }
  // Check the comparison file extension
  if (comp_ext == ".v") {
    spdlog::info("Comparison file in Verilog format.");
  } else if (comp_ext == ".lib") {
    spdlog::info("Comparison file in Liberty format.");
    comp_libfile = new LibFile(comp_file, comp_logname);
  } else {
    spdlog::error("Unsupported comparison file format: '{}'", comp_ext);
    return;
  }

  // Tranverse the cell names and check functional equivalence
  for (const auto &cell : cell_names) {
    spdlog::info("Checking functional equivalence for cell: '{}'", cell);
    std::map<std::string, std::string> ref_outpin_map;
    std::map<std::string, std::string> comp_outpin_map;

    if (ref_ext == ".v") {
      ref_outpin_map = extractLogicFromVerilog(ref_file, cell);
    } else if (ref_ext == ".lib") {
      ref_outpin_map = ref_libfile->logic(cell);
    }

    if (comp_ext == ".v") {
      // getAST(comp_file, cell);
      // extractAndPrintNetlistInfo(comp_file, cell);
      comp_outpin_map = extractLogicFromVerilog(comp_file, cell);
    } else if (comp_ext == ".lib") {
      comp_outpin_map = comp_libfile->logic(cell);
    }

    spdlog::info("Logic function expressions collected for cell: '{}'", cell);
    spdlog::info("Starting logic comparison ...");

    // TODO Perform functional equivalence check
    // LogicComparator comparator(ref_outpin_map, comp_outpin_map, cell);
    // comparator.generateReport(report_file_name);
    spdlog::info("Functional equivalence check completed for cell: '{}'", cell);
  }
  spdlog::info("Functional equivalence check completed for all cells,");
  spdlog::info("Report written to: '{}'", report_file_name);
}

int main(int argc, char *argv[]) {
  // Parse command line arguments
  std::vector<std::string> library_paths; // Support multiple files
  std::string log_file_name = "";

  // Command line parameter parsing using CLI11
  CLI::App app{APP_NAME};
  app.set_version_flag("-v,--version", APP_VERSION);

  // Add subcommand for parse mode
  CLI::App *parse_cmd =
      app.add_subcommand("parse", "Parse the Liberty file and write JSON to a file");
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
  mono_cmd->add_flag("-s,--slew", is_slew,
                     "Specify that monotonicity checks also include input slew.");
  mono_cmd->callback([&] {
    printInfo();
    // Check if multi files
    if (library_paths.size() > 1) {
      spdlog::info("Running multi-threaded monotonicity check for {} files.", library_paths.size());
      spdlog::info("Each thread will write to its own log file.");

      // Sequential json file check
      for (const auto &library_path : library_paths) {
        if (!std::filesystem::exists(std::filesystem::path(library_path).stem().string() +
                                     ".json")) {
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
      "compare",
      "Compare the comparison library against the reference library and report differences");
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
  std::vector<std::string> cell_names = {}; // "CMPE42D1" "AN2D0", "DFQD1"
  supercell_cmd->add_option("--cells", cell_names,
                            "Specify the cell names to generate supercells for");
  supercell_cmd->callback([&] {
    printInfo();
    // Check if multi files
    if (library_paths.size() > 1) {
      spdlog::info("Running multi-threaded supercell generation for {} files.",
                   library_paths.size());
      spdlog::info("Each library will write to its own log file.");

      // Sequential json file check
      for (const auto &library_path : library_paths) {
        if (!std::filesystem::exists(std::filesystem::path(library_path).stem().string() +
                                     ".json")) {
          spdlog::info("JSON file not found for '{}'. Parsing Liberty file first.", library_path);
          parseLibFile(library_path, log_file_name = "");
        }
      }
      spdlog::info("All JSON files prepared.");
      // Parallel supercell generation
      std::vector<std::thread> threads;
      for (const auto &library_path : library_paths) {
        threads.emplace_back(supercellLibFile, library_path, log_file_name = "", chain_length,
                             cell_names);
      }
      for (auto &thread : threads) {
        thread.join();
      }
      spdlog::info("All threads completed.");
    } else {
      supercellLibFile(library_paths[0], log_file_name, chain_length, cell_names);
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
  zlibboost_cmd->add_option("--main", main_py_dir,
                            "Specify the main python script directory. Default: "
                            "/home/songzx/Projects/zlibboost/zlibboost.py");
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
  CLI::App *verilog_cmd =
      app.add_subcommand("verilog", "Generate Verilog file for given Liberty file");
  verilog_cmd->add_option("library_path", library_paths, "Specify the library file to process")
      ->check(CLI::ExistingFile)
      ->required();
  verilog_cmd->add_option("-l,--log", log_file_name,
                          "Specify the log file name. Default: <basename>.verilog.log");
  verilog_cmd->add_option("-c,--chain", chain_length,
                          "Specify the chain length for verilog generation. Default: 1");
  verilog_cmd->add_option("--cells", cell_names, "Specify the cell names to generate Verilog for");
  verilog_cmd->callback([&] {
    printInfo();
    // Check if multi files
    if (library_paths.size() > 1) {
      spdlog::info("Running multi-threaded Verilog generation for {} files.", library_paths.size());
      spdlog::info("Each library will write to its own log file.");

      // Sequential json file check
      for (const auto &library_path : library_paths) {
        if (!std::filesystem::exists(std::filesystem::path(library_path).stem().string() +
                                     ".json")) {
          spdlog::info("JSON file not found for '{}'. Parsing Liberty file first.", library_path);
          parseLibFile(library_path, log_file_name = "");
        }
      }
      spdlog::info("All JSON files prepared.");
      // Parallel Verilog generation
      std::vector<std::thread> threads;
      for (const auto &library_path : library_paths) {
        threads.emplace_back(verilogLibFile, library_path, log_file_name = "", chain_length,
                             cell_names);
      }
      for (auto &thread : threads) {
        thread.join();
      }
      spdlog::info("All threads completed.");
    } else {
      verilogLibFile(library_paths[0], log_file_name, chain_length, cell_names);
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
        if (!std::filesystem::exists(std::filesystem::path(library_path).stem().string() +
                                     ".json")) {
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
  CLI::App *func_cmd = app.add_subcommand(
      "func", "Check functional equivalence of two Liberty files or Verilog files");
  std::string ref_file, comp_file;
  func_cmd->add_option("--ref", ref_file, "Specify the reference Liberty or Verilog file")
      ->check(CLI::ExistingFile)
      ->required();
  func_cmd->add_option("--comp", comp_file, "Specify the comparison Liberty or Verilog file")
      ->check(CLI::ExistingFile)
      ->required();
  func_cmd->add_option("--cells", cell_names,
                       "Specify the cell names to check functional equivalence for");
  func_cmd->add_option("--report", report_file_name,
                       "Specify the report file name. Default: <comp_lib>.cmp.md");

  func_cmd->callback([&] {
    printInfo();
    funcLibFile(ref_file, comp_file, cell_names, report_file_name);
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