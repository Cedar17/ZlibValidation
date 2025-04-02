#include "LibFileOperations.hpp"

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
  logger->set_level(spdlog::level::trace);
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
    report_file_name = std::filesystem::path(comp_file).stem().string() + ".logic.cmp.md";
  } else if (std::filesystem::path(report_file_name).extension() != ".txt" &&
             std::filesystem::path(report_file_name).extension() != ".md") {
    // report file name not end in .txt or .md, warn user and append .md
    report_file_name = report_file_name + ".md";
    spdlog::warn("Report file name does not end in .txt or .md. Report will be written to '{}'",
                 report_file_name);
  }

  // Clear the report file
  std::ofstream report_file(report_file_name);
  if (!report_file.is_open()) {
    spdlog::error("Failed to open report file: '{}'", report_file_name);
    return;
  }
  report_file.close();
  spdlog::info("Report file cleared: '{}'", report_file_name);

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

    LogicComparator comparator(ref_outpin_map, comp_outpin_map, cell);
    // Easter egg
    if (cell == "easteregg") {
      spdlog::info("Easter egg🥚 found! 😍 Running ExprTk Eample 07...");
      comparator.logic();
      return;
    }

    // // Test for preprocessing
    // std::string ref_expr;
    // std::string comp_expr;
    // for (const auto &pin : ref_outpin_map) {
    //   spdlog::info("Pin -> Expression: {} -> {}", pin.first, pin.second);
    //   ref_expr = comparator.preprocessExpression(pin.second);
    // }
    // for (const auto &pin : comp_outpin_map) {
    //   spdlog::info("Pin => Expression: {} => {}", pin.first, pin.second);
    //   comp_expr = comparator.preprocessExpression(pin.second);
    // }
    // std::vector<std::string> sorted_vars;
    // comparator.extractVariables(ref_expr, comp_expr, sorted_vars);
    // struct PinComparisonResult pin_comparison_result;
    // // comp_expr = "not ((not(A1) and not(A2)) or B)";
    // comparator.compareSingleExpressionPair(ref_expr, comp_expr, sorted_vars,
    //                                                pin_comparison_result);

    // comparator.compareCellLogic();
    comparator.compareCellLogic();
    comparator.generateReport(report_file_name);

    spdlog::info("Functional equivalence check completed for cell: '{}'", cell);
  }
  spdlog::info("Functional equivalence check completed for all cells,");
  spdlog::info("Report written to: '{}'", report_file_name);
}
