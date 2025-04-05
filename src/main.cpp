// ./src/main.cpp

#include "CLI/CLI.hpp"

#include "LibFileOperations.hpp"
#include "version.h"

/**
 * @file main.cpp
 * @brief This file contains the main function for the ZlibValidation tool.
 *
 * The ZlibValidation tool is a command-line application that provides several functionalities
 * for processing and validating Liberty files, including parsing, monotonicity checking,
 * comparison, supercell generation, Verilog/SPICE netlist generation, functional equivalence check,
 * and a utility to clear generated files. It uses the CLI11 library for command-line argument parsing
 * and spdlog for logging.
 *
 * The main function parses command-line arguments using CLI11 to determine the desired operation
 * and its parameters. It then calls the appropriate functions to perform the requested task.
 * The tool supports processing multiple Liberty files in parallel using multi-threading for
 * monotonicity checking, supercell generation, Verilog generation, and SPICE generation.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line argument strings.
 * @return 0 if the program executes successfully, or an error code otherwise.
 *
 * @details
 * The main function performs the following steps:
 *   1. Initializes command-line argument parsing using CLI11.
 *   2. Defines subcommands for each supported operation:
 *     - `parse`: Parses a Liberty file and writes JSON to a file.
 *     - `mono`: Checks the monotonicity of timing arc values in a Liberty file.
 *     - `compare`: Compares two Liberty files and reports differences.
 *     - `supercell`: Generates supercells for a given Liberty file.
 *     - `zlibboost`: Runs the ZlibBoost tool for multi-threaded library processing.
 *     - `clear`: Clears log, JSON, map, markdown, Verilog, and SPICE files in the current directory.
 *     - `verilog`: Generates Verilog netlist for a given Liberty file.
 *     - `spice`: Generates SPICE netlist for a given Liberty file.
 *     - `func`: Checks functional equivalence of two Liberty or Verilog files.
 *   3. Parses the command-line arguments using CLI11.
 *   4. Calls the appropriate function based on the selected subcommand.
 *   5. Logs the program's exit status and timestamp.
 *
 * @note The tool relies on external libraries such as CLI11, spdlog, and potentially others
 *       depending on the specific operation being performed. Ensure that these libraries are
 *       properly installed and configured before running the tool.
 */
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
      "Compare the comparison library against the reference one and report differences");
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
  CLI::App *clear_cmd = app.add_subcommand(
      "clear", "Clear the log, JSON, map, markdown, Verilog, SPICE files in this directory");
  clear_cmd->callback([&] {
    printInfo();
    std::filesystem::path current_dir = std::filesystem::current_path();
    for (const auto &entry : std::filesystem::directory_iterator(current_dir)) {
      if (entry.path().extension() == ".log" || entry.path().extension() == ".json" ||
          entry.path().extension() == ".map" || entry.path().extension() == ".md" ||
          entry.path().extension() == ".v" || entry.path().extension() == ".spi") {
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
  spice_cmd->add_option("-c,--chain", chain_length,
                        "Specify the chain length for SPICE generation. Default: 1");
  spice_cmd->add_option("--cells", cell_names, "Specify the cell names to generate SPICE for");
  std::string verilog_lib_file =
      "/home/songzx/examples/mypdk/TSMC65/TSMC65NM_CLN65LP_STDIO_STDCELL/tcbn65lp_220a/"
      "0396011_20170308/TSMCHOME/digital/Front_End/verilog/tcbn65lp.v";
  std::string spice_lib_file =
      "/home/songzx/examples/mypdk/TSMC65/TSMC65NM_CLN65LP_STDIO_STDCELL/tcbn65lp_220a/"
      "0396011_20170308/TSMCHOME/digital/Back_End/lpe_spice/tcbn65lp_200a/tcbn65lp_200a_lpe.spi";
  spice_cmd->add_option("--vl", verilog_lib_file,
                        "Specify the location of the Verilog primitive library file");
  spice_cmd->add_option(
      "--sl", spice_lib_file,
      "Specify the location of the SPICE library file to be included in the output");
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
        threads.emplace_back(spiceLibFile, library_path, log_file_name = "", chain_length,
                             cell_names, verilog_lib_file, spice_lib_file);
      }
      for (auto &thread : threads) {
        thread.join();
      }
      spdlog::info("All threads completed.");
    } else {
      spiceLibFile(library_paths[0], log_file_name, chain_length, cell_names, verilog_lib_file,
                   spice_lib_file);
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