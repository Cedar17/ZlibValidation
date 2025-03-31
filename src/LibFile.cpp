#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "Iterators.hpp"
#include "json_utils.hpp"
#include "LibFile.hpp"
#include "verilog_utils.hpp"

/**
 * @brief Constructs a LibFile object with specified file path and logger name
 *
 * This constructor initializes a LibFile object with the given file path and creates
 * a logger with the specified name. It sets up:
 * - Two logging sinks: a colored console sink and a file sink
 * - File path information including filename, basename, and a corresponding JSON filename
 * - Log levels (info for console, debug for file)
 *
 * @param filepath The path to the file to be processed
 * @param loggername The name for the logger and the log file
 */
LibFile::LibFile(const std::string &filepath, const std::string &loggername)
    : filepath_(filepath), loggername_(loggername) {
  filename_ = filepath_.string();
  basename_ = filepath_.stem().string();
  jsonname_ = basename_ + ".json";

  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::info);

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(loggername_, true);
  file_sink->set_level(spdlog::level::debug);

  std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
  logger_ = std::make_shared<spdlog::logger>(loggername_, sinks.begin(), sinks.end());
  logger_->set_level(spdlog::level::debug);

  logger_->info("Created LibFile object for '{}'", filename_);
  logger_->info("Debug log in: '{}'", loggername_);
}

LibFile::~LibFile() { logger_->info("Closing file: '{}'", filename_); }

/**
 * @brief Writes the JSON data stored in the object to a file.
 *
 * This method opens the file specified by jsonname_ and writes the content of lib_json_
 * with an indentation of 2 spaces. If the file cannot be opened, an error message is logged.
 * Upon successful writing, an informational message is logged.
 *
 * @throws None directly, but std::ofstream operations may throw exceptions
 */
void LibFile::writeJsonToFile() {
  std::ofstream out(jsonname_);
  if (!out.is_open()) {
    logger_->error("Could not open file '{}' for writing", jsonname_);
    return;
  }
  out << lib_json_.dump(2);
  out.close();
  logger_->info("JSON data written to '{}'", jsonname_);
}

/**
 * @brief Reads the Liberty file specified by the filename_ member variable.
 *
 * This function attempts to read a Liberty file and logs the process. It measures
 * the time taken to read the file and logs any errors encountered during the read
 * operation. If an error occurs, the function logs the error and terminates the program.
 *
 * @details
 * - Logs the start of the read operation.
 * - Measures the duration of the read operation.
 * - Checks for specific errors (invalid name or syntax error) and logs them.
 * - Terminates the program with specific exit codes if errors are detected.
 * - Logs the completion of the read operation and the time taken.
 *
 * @note This function uses the spdlog library for logging and the si2drReadLibertyFile
 * function for reading the Liberty file.
 *
 * @throws This function will terminate the program with exit codes 301 or 401
 * if specific errors are encountered.
 */
void LibFile::read() {
  logger_->info("Reading '{}' ...", filename_);

  auto start = std::chrono::high_resolution_clock::now();
  si2drReadLibertyFile(const_cast<char *>(filepath_.c_str()), &err_);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = end - start;

  if (err_ == SI2DR_INVALID_NAME) {
    logger_->error("Could not open file '{}' for parsing, quitting...", filename_);
    exit(301);
  } else if (err_ == SI2DR_SYNTAX_ERROR) {
    logger_->error("Syntax Errors were detected in the input file!");
    exit(401);
  } else {
    logger_->info("Done. Read time: {:.2f} seconds", duration.count());
  }
}

/**
 * @brief Parses the Liberty file and extracts library information into a JSON structure.
 *
 * This method handles the parsing of a Liberty file by:
 * 1. Initializing the SI2 parser interface error handler
 * 2. Reading the Liberty file contents
 * 3. Extracting the library name
 * 4. Processing second-level groups including:
 *    - Cell definitions, which are added to the JSON structure
 *    - Operating conditions, extracting PVT (Process, Voltage, Temperature) values
 *
 * The parsed data is stored in the lib_json_ member variable, with the following structure:
 *   - "library_name": Name of the library
 *   - "cells": Array of cell definitions
 *   - "process": Process value
 *   - "voltage": Voltage value (rounded to 2 decimal places)
 *   - "temperature": Temperature value
 *
 * @note This method creates local scopes to ensure proper cleanup of SI2 Iterators.
 */
void LibFile::parse() {
  si2drPIInit(&err_); // Initialize private error handler
  this->read();       // Read the Liberty file

  logger_->info("Parsing '{}' ...", filename_);
  // Create a scope for the top-level groups iteration
  {
    GroupsIterator group_iter(si2drPIGetGroups(&err_), err_);
    for (; !group_iter.end(); group_iter.next()) {
      LibGroup lib_group = group_iter.get();

      if (lib_group.getName() != "") {
        libname_ = lib_group.getName();
        lib_json_["library_name"] = this->libname_;
        logger_->info("Library Name: {}", libname_);
      } else {
        logger_->warn("Library Name: <NONAME>");
      }
      // Second level groups
      GroupsIterator sub_group_iter(lib_group.getGroups(), err_);
      for (; !sub_group_iter.end(); sub_group_iter.next()) {
        LibGroup lib_sub_group = sub_group_iter.get();

        std::string sub_group_type = lib_sub_group.getType();
        std::string sub_group_name = lib_sub_group.getName();

        if (sub_group_type == "cell") {
          logger_->debug("Cell Name: {}", sub_group_name);
          // if (sub_group_name != "AN2D0") {
          //   continue;
          // }
          // Handle each cell
          json cell_json = generateCellJson(lib_sub_group, err_);
          lib_json_["cells"].push_back(cell_json);
        } else if (sub_group_type == "operating_conditions") {
          logger_->info("Operating Conditions: {}", sub_group_name);
          // Get PVT from Operating Conditions
          AttributesIterator attr_iter(lib_sub_group.getAttrs(), err_);
          for (; !attr_iter.end(); attr_iter.next()) {
            LibAttribute lib_attr = attr_iter.get();
            logger_->debug("Attribute Name: {}", lib_attr.getName());
            logger_->debug("Int Value: {}", lib_attr.getInt());
            logger_->debug("Float Value: {}", lib_attr.getFloat());
            logger_->debug("String Value: {}", lib_attr.getString());
            if (lib_attr.getName() == "process") {
              process_ = lib_attr.getInt();
              lib_json_["process"] = process_;
            } else if (lib_attr.getName() == "voltage") {
              voltage_ = lib_attr.getFloat();
              // Round to 2 decimal places to avoid floating-point precision issues
              lib_json_["voltage"] = std::round(voltage_ * 100) / 100.0;
            } else if (lib_attr.getName() == "temperature") {
              temperature_ = lib_attr.getInt();
              lib_json_["temperature"] = temperature_;
            }
          }
          logger_->info("P: {}, V: {}, T: {}", process_, voltage_, temperature_);
        }
        // sub_group_iter's lifetime ends here, and the destructor is called
      }
    }
    // group_iter's lifetime ends here, and the destructor is called
  }
  si2drPIQuit(&err_);
}

void LibFile::modify() { logger_->info("Modifying the file..."); }

/**
 * @brief Checks if the values in a timing arc are monotonically increasing.
 *
 * This function examines the values in the specified timing arc to ensure they are monotonically
 * increasing. It checks monotonicity in two dimensions:
 * 1. Across rows (by output load capacitance)
 * 2. Across columns (by input slew, only if is_slew is true)
 *
 * The function logs detailed information about any non-monotonic values found, including
 * cell name, pin names, and conditional statements (when clause) if present.
 *
 * @param cell The JSON object representing the cell being checked
 * @param pin The JSON object representing the pin being checked
 * @param arc The JSON object representing the timing arc being checked
 * @param timing_arc_name The name of the timing arc to check (e.g., "cell_rise", "cell_fall")
 * @param is_slew Boolean flag indicating whether to check monotonicity across input slew values
 *
 * @return true if all values in the timing arc are monotonic, false otherwise
 *
 * @throw None, but logs errors or warnings for invalid data formats or non-monotonic values
 */
bool LibFile::checkTimingArcMonotonicity(const json &cell, const json &pin, const json &arc,
                                         const std::string &timing_arc_name, const bool is_slew) {
  if (arc.contains("when") && !arc["when"].get<std::string>().empty()) {
    logger_->debug(
        "Checking cell: '{}', pin: '{}', related_pin: '{}', timing_arc: '{}', when: '{}'",
        cell["cell_name"].get<std::string>(), pin["pin_name"].get<std::string>(),
        arc["related_pin"].get<std::string>(), timing_arc_name, arc["when"].get<std::string>());
  } else {
    logger_->debug("Checking cell: '{}', pin: '{}', related_pin: '{}', timing_arc: '{}'",
                   cell["cell_name"].get<std::string>(), pin["pin_name"].get<std::string>(),
                   arc["related_pin"].get<std::string>(), timing_arc_name);
  }
  bool is_monotonic = true; // Assume the values are monotonic initially
  if (arc.contains(timing_arc_name) && arc[timing_arc_name].contains("values")) {
    std::vector<std::vector<double>> value_matrix;
    // Generate a matrix of values from the JSON array
    for (const auto &row_val : arc[timing_arc_name]["values"]) {
      std::vector<double> row_data;
      // Check if the value is an array
      if (!row_val.is_array()) {
        logger_->error("Invalid format: '{}' values should be an array of arrays in cell '{}', pin "
                       "'{}', related_pin '{}'",
                       timing_arc_name, cell["cell_name"].get<std::string>(),
                       pin["pin_name"].get<std::string>(), arc["related_pin"].get<std::string>());
        return false;
      }
      // Check for non-numeric values
      for (const auto &val : row_val) {
        if (val.is_number()) {
          row_data.push_back(val.get<double>());
        } else {
          logger_->warn("Non-numeric value found in '{}'.values, skipping value: {} in cell '{}', "
                        "pin '{}', related_pin '{}'",
                        timing_arc_name, val.dump(), cell["cell_name"].get<std::string>(),
                        pin["pin_name"].get<std::string>(), arc["related_pin"].get<std::string>());
        }
      }
      value_matrix.push_back(row_data);
    }
    // Check the matrix for monotonicity
    if (!value_matrix.empty() && !value_matrix[0].empty()) {
      // Check the monotonic incrementality of rows (by output load capacitance, index 2)
      for (size_t i = 0; i < value_matrix.size(); ++i) {
        for (size_t j = 1; j < value_matrix[i].size(); ++j) {
          if ((value_matrix[i][j] < value_matrix[i][j - 1] &&
               pin["pin_name"] != arc["related_pin"]) ||
              (value_matrix[i][j] == 0 && value_matrix[i][j - 1] == 0)) {
            // If contains "when" key, log the when value
            if (arc.contains("when") && !arc["when"].get<std::string>().empty()) {
              logger_->warn("Non-monotonic (by load) '{}' values: ({}, {}) {} < ({}, {}) {} "
                            "for Cell: {} Pin: {}->{} when: \"{}\"",
                            timing_arc_name, i, j, value_matrix[i][j], i, j - 1,
                            value_matrix[i][j - 1], cell["cell_name"].get<std::string>(),
                            arc["related_pin"].get<std::string>(),
                            pin["pin_name"].get<std::string>(), arc["when"].get<std::string>());
            } else {
              logger_->warn("Non-monotonic (by load) '{}' values: ({}, {}) {} < ({}, {}) {} "
                            "for Cell: {} Pin: {}->{}",
                            timing_arc_name, i, j, value_matrix[i][j], i, j - 1,
                            value_matrix[i][j - 1], cell["cell_name"].get<std::string>(),
                            arc["related_pin"].get<std::string>(),
                            pin["pin_name"].get<std::string>());
            }
            is_monotonic = false;
          }
        }
      }
      if (is_slew) {
        // Check the monotonic incrementality of columns (by input slew, index 1)
        for (size_t j = 0; j < value_matrix[0].size(); ++j) {
          for (size_t i = 1; i < value_matrix.size(); ++i) {
            if ((value_matrix[i][j] < value_matrix[i - 1][j] &&
                 pin["pin_name"] != arc["related_pin"]) ||
                (value_matrix[i][j] == 0 && value_matrix[i - 1][j] == 0)) {
              // If contains "when" key, log the when value
              if (arc.contains("when") && !arc["when"].get<std::string>().empty()) {
                logger_->warn("Non-monotonic (by slew) '{}' values: ({}, {}) {} < ({}, {}) {} "
                              "for Cell: {} Pin: {}->{} when: \"{}\"",
                              timing_arc_name, i, j, value_matrix[i][j], i - 1, j,
                              value_matrix[i - 1][j], cell["cell_name"].get<std::string>(),
                              arc["related_pin"].get<std::string>(),
                              pin["pin_name"].get<std::string>(), arc["when"].get<std::string>());
              } else {
                logger_->warn("Non-monotonic (by slew) '{}' values: ({}, {}) {} < ({}, {}) {} "
                              "for Cell: {} Pin: {}->{}",
                              timing_arc_name, i, j, value_matrix[i][j], i - 1, j,
                              value_matrix[i - 1][j], cell["cell_name"].get<std::string>(),
                              arc["related_pin"].get<std::string>(),
                              pin["pin_name"].get<std::string>());
              }
              is_monotonic = false;
            }
          }
        }
      }
    } else {
      logger_->warn(
          "Empty or invalid 'values' array found for cell: '{}', pin: '{}', related_pin: '{}', "
          "timing_arc: '{}'",
          cell["cell_name"].get<std::string>(), pin["pin_name"].get<std::string>(),
          arc["related_pin"].get<std::string>(), timing_arc_name);
    }
  }
  return is_monotonic;
}

/**
 * @brief Validates that lookup tables are monotonically increasing with respect to the output load.
 *
 * This function checks the following timing data in the current library to ensure monotonicity:
 * - cell_rise and retaining_rise
 * - cell_fall and retaining_fall
 * - rise_transition and retain_rise_slew
 * - fall_transition and retain_fall_slew
 * - min_pulse_width constraints (rise_constraint, fall_constraint)
 *
 * The function first checks if a parsed JSON representation exists. If not, it parses the Liberty
 * file and creates the JSON file. It then evaluates each timing arc in all cells and reports
 * the pass/fail status at the end.
 *
 * @param is_slew Boolean flag indicating whether to check slew values rather than delay values
 *
 * @details The function outputs:
 *          - Number of cells that passed/failed the monotonicity check
 *          - List of cells that failed the check (if any)
 */
void LibFile::mono(const bool is_slew) {
  /*
   * Use this command to check the following data in the current library to ensure
   * the tables are monotonically increasing with respect to output load:
   * cell_rise retaining_rise
   * cell_fall retaining_fall
   * rise_transition retain_rise_slew
   * fall_transition retain_fall_slew
   * mpw
   */
  logger_->info("Monotonicity check of '{}' starting ...", filename_);

  if (!std::filesystem::exists(jsonname_)) {
    logger_->info("JSON file not found. Parsing Liberty file first.");
    this->parse();
    this->writeJsonToFile();
  } else {
    // Read the JSON file into json object
    std::ifstream in(jsonname_);
    if (!in.is_open()) {
      logger_->error("Could not open file '{}' for reading", jsonname_);
      return;
    }
    try {
      lib_json_ = json::parse(in);
    } catch (const json::parse_error &e) {
      logger_->error("JSON parsing error in file '{}': {}", jsonname_, e.what());
      in.close();
      return;
    }
    in.close();
  }

  std::map<std::string, bool> cell_monotonicity_status; // Track pass/fail status for each cell
  std::vector<std::string> failed_cells;                // List of failed cell names
  int total_cells = 0;
  int passed_cells = 0;

  // Check the monotonicity of delay values
  // The index 1 (time values) must be monotonically increasing and >= 0
  for (const auto &cell : lib_json_["cells"]) {
    total_cells++;
    bool cell_is_monotonic = true; // Assume cell is monotonic initially
    std::string cell_name = cell["cell_name"].get<std::string>();
    cell_monotonicity_status[cell_name] =
        true; // Initialize to pass, will be set to false if any check fails

    if (cell.contains("output_pins")) {
      for (const auto &pin : cell["output_pins"]) {
        if (pin.contains("timing_arcs")) {
          for (const auto &arc : pin["timing_arcs"]) {
            // Check 4 timing arc names and accumulate monotonicity status
            for (const auto &name :
                 {"cell_rise", "cell_fall", "rise_transition", "fall_transition"}) {
              if (!checkTimingArcMonotonicity(cell, pin, arc, name, is_slew)) {
                cell_is_monotonic = false;
              }
            }
          }
        }
      }
    }
    if (cell.contains("input_pins")) {
      for (const auto &pin : cell["input_pins"]) {
        if (!pin.contains("clock")) {
          continue; // Skip non-clock pins
        }
        // logger_->debug("Clock pin: {}", pin["pin_name"].get<std::string>());
        if (pin.contains("timing_arcs")) {
          for (const auto &arc : pin["timing_arcs"]) {
            if (arc.contains("timing_type")) {
              // logger_->debug("Timing type: {}", arc["timing_type"].get<std::string>());
              if (arc["timing_type"].get<std::string>() == "min_pulse_width") {
                for (const auto &name : {"rise_constraint", "fall_constraint"}) {
                  if (!checkTimingArcMonotonicity(cell, pin, arc, name, is_slew)) {
                    cell_is_monotonic = false;
                  }
                }
              }
            }
          }
        }
      }
    }
    // Update cell status based on all checks
    cell_monotonicity_status[cell_name] = cell_is_monotonic;
    if (cell_is_monotonic) {
      passed_cells++;
    } else {
      failed_cells.push_back(cell_name);
    }
  }
  logger_->info("Monotonicity check of '{}' completed.", filename_);

  // Output summary statistics
  logger_->info("validate_monotonicity : {} out of {} cells passed", passed_cells, total_cells);
  logger_->info("validate_monotonicity : {} out of {} cells failed", total_cells - passed_cells,
                total_cells);
  if (!failed_cells.empty()) {
    std::stringstream failed_cells_ss;
    for (size_t i = 0; i < failed_cells.size(); ++i) {
      failed_cells_ss << failed_cells[i];
      if (i < failed_cells.size() - 1) {
        failed_cells_ss << ", "; // Add comma if not the last element
      }
    }
    logger_->info("Failed cell list : {}", failed_cells_ss.str());
  }
}

/**
 * @brief Creates supercells based on standard cells in the library file.
 *
 * This method creates supercells by combining standard cells in chains.
 * For each input-output pin pair of a cell, it creates a supercell entry
 * with naming format: <cellname>__X<chain_length>__<input_pin>__<output_pin>.
 * The results are written to a .map file.
 *
 * For sequential cells (with clock pins), the chain length is always set to 1
 * regardless of the requested chain length.
 *
 * @param chain_length The length of the chain for combinational cells
 * @param cell_names Vector of specific cell names to process. If empty, all cells will be
 * processed.
 *
 * @note Before creating supercells, this method checks for the existence of
 *       a JSON representation of the Liberty file and parses it if not found.
 * @note The method will report any requested cells that were not found in the library.
 */
void LibFile::supercell(const int chain_length, const std::vector<std::string> &cell_names) {
  /*
   * Supercells are named as follows:
   *   <cellname>__X<chain_length>__<input_pin>__<output_pin>
   */
  logger_->info("Creating supercells for '{}'", filename_);

  if (!std::filesystem::exists(jsonname_)) {
    logger_->info("JSON file not found. Parsing Liberty file first.");
    this->parse();
    this->writeJsonToFile();
  } else {
    // Read the JSON file into json object
    std::ifstream in(jsonname_);
    if (!in.is_open()) {
      logger_->error("Could not open file '{}' for reading", jsonname_);
      return;
    }
    try {
      lib_json_ = json::parse(in);
    } catch (const json::parse_error &e) {
      logger_->error("JSON parsing error in file '{}': {}", jsonname_, e.what());
      in.close();
      return;
    }
    in.close();
  }

  // write to .map file
  std::ofstream out(basename_ + ".map");
  if (!out.is_open()) {
    logger_->error("Could not open file '{}' for writing", basename_ + ".map");
    return;
  }
  // if celll_names is empty, create supercells for all cells
  // else create supercells for only the specified cells

  // Check if we're processing specific cells or all cells
  bool process_all_cells = cell_names.empty();

  if (process_all_cells) {
    logger_->info("Creating supercells for ALL cells in '{}'", filename_);
  } else {
    logger_->info("Creating supercells for {} specified cells in '{}'", cell_names.size(),
                  filename_);
  }

  // Create a set for faster lookups if we have specific cell names
  std::unordered_set<std::string> cell_set;
  std::unordered_set<std::string> found_cells;
  if (!process_all_cells) {
    cell_set.insert(cell_names.begin(), cell_names.end());
  }

  for (const auto &cell : lib_json_["cells"]) {
    bool is_sequential = false;
    std::string cell_name = cell["cell_name"].get<std::string>();

    // Skip cells not in the specified list
    if (!process_all_cells && cell_set.find(cell_name) == cell_set.end()) {
      continue;
    }

    // Mark this cell as found
    if (!process_all_cells) {
      found_cells.insert(cell_name);
    }

    logger_->debug("Creating supercells for cell: '{}'", cell_name);

    std::vector<std::string> output_pins;
    std::vector<std::string> input_pins;
    // Extract input and output pins
    if (cell.contains("output_pins")) {
      for (const auto &pin : cell["output_pins"]) {
        output_pins.push_back(pin["pin_name"].get<std::string>());
      }
    }
    if (cell.contains("input_pins")) {
      for (const auto &pin : cell["input_pins"]) {
        if (pin.contains("clock")) {
          is_sequential = true;
          // clock pin not use for supercell
          continue;
        }
        input_pins.push_back(pin["pin_name"].get<std::string>());
      }
    }

    // create supercells for all combinations of input and output pins
    for (const auto &output_pin : output_pins) {
      for (const auto &input_pin : input_pins) {
        if (is_sequential) {
          const int chain_len = 1;
          std::string supercell_name =
              cell_name + "__X" + std::to_string(chain_len) + "__" + input_pin + "__" + output_pin;
          out << cell_name << " " << supercell_name << std::endl;
        } else {
          std::string supercell_name = cell_name + "__X" + std::to_string(chain_length) + "__" +
                                       input_pin + "__" + output_pin;
          out << cell_name << " " << supercell_name << std::endl;
        }
      }
    }
  }

  // Check for cells that were specified but not found
  if (!process_all_cells) {
    std::vector<std::string> not_found_cells;
    for (const auto &requested_cell : cell_names) {
      if (found_cells.find(requested_cell) == found_cells.end()) {
        not_found_cells.push_back(requested_cell);
      }
    }

    // Output warning for cells that weren't found
    if (!not_found_cells.empty()) {
      std::string missing_cells = not_found_cells[0];
      for (size_t i = 1; i < not_found_cells.size(); ++i) {
        missing_cells += ", " + not_found_cells[i];
      }
      logger_->warn("{} specified cell{} not found in the library: {}", not_found_cells.size(),
                    not_found_cells.size() > 1 ? "s were" : " was", missing_cells);
    }
  }

  out.close();
  logger_->info("Supercell creation complete in '{}'", basename_ + ".map");
}

/**
 * @brief Generates Verilog files for cell validation based on the library file.
 *
 * This function creates Verilog modules for each cell in the specified chain and
 * generates a top-level module that connects them all together. The process involves:
 *
 * 1. Creating supercells based on the specified chain length and cell names
 * 2. Reading the mapping file to identify supercell name relationships
 * 3. Generating individual Verilog modules for each supercell, handling both
 *    sequential and combinational cells differently
 * 4. Creating ANSI-style port definitions for each module
 * 5. Using the slang library to build proper syntax trees for Verilog modules
 * 6. Creating a top-level module that instantiates all the individual modules
 * 7. Writing the complete Verilog output to the output file
 *
 * Sequential cells are instantiated once, while combinational cells are instantiated
 * multiple times based on the specified chain length.
 *
 * @param chain_length The number of times to chain combinational cells together
 * @param cell_names Vector of cell names to include in the Verilog generation
 *
 * @note The function writes a temporary file during processing that might not be removed
 *       when the function completes (commented out cleanup code).
 */
void LibFile::verilog(const int chain_length, const std::vector<std::string> &cell_names) {
  logger_->info("Creating Verilog for '{}'", filename_);

  this->supercell(chain_length, cell_names);

  // Create a temporary file for individual modules
  std::string temp_file = basename_ + "_temp.v";
  std::ofstream out(temp_file);
  if (!out.is_open()) {
    logger_->error("Could not open temp file '{}' for writing", temp_file);
    return;
  }

  // Read the .map file into a vector to preserve all entries and their order
  std::vector<std::pair<std::string, std::string>> supercell_entries;
  std::ifstream in(basename_ + ".map");
  if (!in.is_open()) {
    logger_->error("Could not open file '{}' for reading", basename_ + ".map");
    return;
  }

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string cell_name, supercell_name;
    if (!(iss >> cell_name >> supercell_name)) {
      logger_->warn("Invalid line format in map file: '{}'", line);
      continue;
    }
    supercell_entries.push_back({cell_name, supercell_name});
  }
  in.close();
  logger_->info("Read {} supercells from '{}'", supercell_entries.size(), basename_ + ".map");

  // Store module information for top-level creation
  std::vector<std::string> module_names;
  std::map<std::string, std::vector<std::string>> module_inputs;
  std::map<std::string, std::vector<std::string>> module_outputs;

  // Generate verilog module for each supercell
  for (const auto &supercell_entry : supercell_entries) {
    // Check if the cell is sequential
    bool is_sequential = false;
    // Count the number of instances will be created in verilog
    int instance_count = 0;
    // Get original cell name from pair
    const std::string &cell_name = supercell_entry.first;
    // Get supercell name(as well as module name) from pair
    const std::string &module_name = supercell_entry.second;

    // Store module name for top-level creation
    module_names.push_back(module_name);

    logger_->info("Creating Module: '{}' from Cell '{}", module_name, cell_name);

    // Get the cell JSON object
    json cell_json;
    for (const auto &cell : lib_json_["cells"]) {
      if (cell["cell_name"].get<std::string>() == cell_name) {
        cell_json = cell;
        break;
      }
    }

    // Get input/output pins from the cell JSON object
    std::vector<std::string> input_pins;
    std::vector<std::string> output_pins;
    std::stringstream input_pins_ss;
    std::stringstream output_pins_ss;
    if (cell_json.contains("input_pins")) {
      for (const auto &pin : cell_json["input_pins"]) {
        if (pin.contains("clock")) {
          is_sequential = true;
        }
        std::string pin_name = pin["pin_name"].get<std::string>();
        input_pins.push_back(pin_name);
        input_pins_ss << pin_name << ", ";

        // Store input pin name for top-level creation
        module_inputs[module_name].push_back(pin_name);
      }
    }
    if (cell_json.contains("output_pins")) {
      for (const auto &pin : cell_json["output_pins"]) {
        std::string pin_name = pin["pin_name"].get<std::string>();
        output_pins.push_back(pin_name);
        output_pins_ss << pin_name << ", ";

        // Store output pin name for top-level creation
        module_outputs[module_name].push_back(pin_name);
      }
    }
    logger_->debug("Input pins set: {}", input_pins_ss.str());
    logger_->debug("Output pins set: {}", output_pins_ss.str());

    // Sequential cells will have only one instance
    // Combinational cells will have instances equal to chain length
    if (is_sequential) {
      instance_count = 1;
    } else {
      instance_count = chain_length;
    }

    // Create ANSI port list
    std::string port_list_text = "(";
    // Add input ports
    bool isFirstPort = true;
    for (const auto &input_pin : input_pins) {
      if (!isFirstPort) {
        port_list_text += ", ";
      }
      port_list_text += "input " + input_pin;
      isFirstPort = false;
    }
    // Add output ports
    for (const auto &output_pin : output_pins) {
      if (!isFirstPort) {
        port_list_text += ", ";
      }
      port_list_text += "output " + output_pin;
      isFirstPort = false;
    }
    port_list_text += ")";
    logger_->debug("ANSI Port list: {}", port_list_text);

    // Creat full module header text
    std::string fullModuleText = "module " + module_name + port_list_text + ";\nendmodule";
    logger_->debug("Full module text: {}", fullModuleText);

    // Generate the syntax tree for the module using slang library
    auto tree = slang::syntax::SyntaxTree::fromText(fullModuleText);
    if (tree) {
      // Add ports to the syntax tree
      ModuleRewriter rewriter(input_pins, output_pins, supercell_entry, instance_count, logger_);

      tree = rewriter.transform(tree);
      // Revisit the syntax tree to check architecture
      rewriter.visit(tree->root());

      // Output the transformed syntax tree
      out << "`timescale 1ns/10ps" << std::endl;
      out << slang::syntax::SyntaxPrinter::printFile(*tree) << std::endl << std::endl;
    } else {
      logger_->error("Failed to create syntax tree for module '{}'", module_name);
      continue;
    }
  }

  out.close();

  // Create the top-level module
  std::ofstream final_out(basename_ + ".v");
  if (!final_out.is_open()) {
    logger_->error("Could not open file '{}' for writing", basename_ + ".v");
    return;
  }

  // Collect all port names separately for inputs and outputs
  std::vector<std::string> all_input_ports;
  std::vector<std::string> all_output_ports;
  std::stringstream top_instances;

  // First pass: collect all port names
  for (const auto &module_name : module_names) {
    // Collect input port names
    for (const auto &pin : module_inputs[module_name]) {
      all_input_ports.push_back(module_name + "__" + pin);
    }

    // Collect output port names
    for (const auto &pin : module_outputs[module_name]) {
      all_output_ports.push_back(module_name + "__" + pin);
    }

    // Create instance
    std::string instance_name = "I_" + module_name.substr(0, module_name.find("__X"));
    for (size_t i = module_name.find("__X") + 3; i < module_name.length(); i++) {
      if (module_name[i] == '_' && module_name[i + 1] == '_') {
        instance_name += "__" + module_name.substr(i + 2);
        break;
      }
    }

    // Generate port connections for this instance
    std::stringstream instance_ports;
    for (const auto &pin : module_inputs[module_name]) {
      instance_ports << "." << pin << "(" << module_name << "__" << pin << "), ";
    }
    for (const auto &pin : module_outputs[module_name]) {
      instance_ports << "." << pin << "(" << module_name << "__" << pin << "), ";
    }

    // Remove last comma and space
    std::string instance_ports_str = instance_ports.str();
    if (!instance_ports_str.empty()) {
      instance_ports_str = instance_ports_str.substr(0, instance_ports_str.length() - 2);
    }

    top_instances << "  " << module_name << "  " << instance_name << " (" << instance_ports_str
                  << ");\n";
  }

  // Now generate the port list with all inputs first, then all outputs
  std::stringstream top_ports;

  // Add all input ports first
  for (size_t i = 0; i < all_input_ports.size(); ++i) {
    top_ports << all_input_ports[i];
    if (i < all_input_ports.size() - 1 || !all_output_ports.empty()) {
      top_ports << ", ";
    }
  }

  // Then add all output ports
  for (size_t i = 0; i < all_output_ports.size(); ++i) {
    top_ports << all_output_ports[i];
    if (i < all_output_ports.size() - 1) {
      top_ports << ", ";
    }
  }

  // Generate input and output declarations
  std::stringstream top_inputs;
  std::stringstream top_outputs;

  // Generate input declarations
  for (const auto &port : all_input_ports) {
    top_inputs << "  input " << port << ";\n";
  }

  // Generate output declarations
  for (const auto &port : all_output_ports) {
    top_outputs << "  output " << port << ";\n";
  }

  // Write the top module
  final_out << "`timescale 1ns/10ps" << std::endl;
  final_out << "module validate_top ( " << top_ports.str() << " );" << std::endl;
  final_out << std::endl; // Add newline before port declarations
  final_out << top_inputs.str();
  final_out << top_outputs.str();
  final_out << std::endl;
  final_out << top_instances.str();
  final_out << "endmodule" << std::endl << std::endl;

  // Append the module definitions from the temporary file
  std::ifstream temp_in(temp_file);
  if (temp_in.is_open()) {
    final_out << temp_in.rdbuf();
    temp_in.close();
  } else {
    logger_->error("Could not open temp file '{}' for reading", temp_file);
  }

  final_out.close();

  // Remove temporary file
  // if (std::filesystem::exists(temp_file)) {
  //   std::filesystem::remove(temp_file);
  // }

  logger_->info("Verilog creation complete in '{}'", basename_ + ".v");
}

std::map<std::string, std::string> LibFile::logic(const std::string &cell_name) {
  std::map<std::string, std::string> logic_map = {};

  logger_->info("Getting output pin logic function for '{}'", filename_);

  if (!std::filesystem::exists(jsonname_)) {
    logger_->info("JSON file not found. Parsing Liberty file first.");
    this->parse();
    this->writeJsonToFile();
  } else {
    // Read the JSON file into json object
    std::ifstream in(jsonname_);
    if (!in.is_open()) {
      logger_->error("Could not open file '{}' for reading", jsonname_);
      return logic_map;
    }
    try {
      lib_json_ = json::parse(in);
    } catch (const json::parse_error &e) {
      logger_->error("JSON parsing error in file '{}': {}", jsonname_, e.what());
      in.close();
      return logic_map;
    }
    in.close();
  }

  // Check if the cell name exists in the JSON object
  for (const auto &cell : lib_json_["cells"]) {
    if (cell["cell_name"].get<std::string>() == cell_name) {
      logger_->debug("Found cell: '{}'", cell_name);

      // Tranverse all output pins
      if (cell.contains("output_pins")) {
        for (const auto &pin : cell["output_pins"]) {
          std::string pin_name = pin["pin_name"].get<std::string>();
          // Check if the pin has a "timing_arcs" attribute
          if (pin.contains("function")) {
            std::string function = pin["function"].get<std::string>();
            logic_map[pin_name] = function;
            logger_->debug("Pin: '{}' Logic Function: '{}'", pin_name, function);
          } else {
            logger_->warn("Pin: '{}' does not have a 'function' attribute", pin_name);
          }
        }
      }
    }
  }
  if (logic_map.empty()) {
    logger_->warn("No logic functions found for cell: '{}'", cell_name);
  } else {
    logger_->info("Found {} Logic functions for cell: '{}'", logic_map.size(), cell_name);
    // Print the logic functions
    for (const auto &pair : logic_map) {
      logger_->info("Pin -> Logic Function: {} -> {}", pair.first, pair.second);
    }
  }
  return logic_map;
}