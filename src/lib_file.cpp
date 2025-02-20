#include <chrono>
#include <fstream>
#include <sstream>

#include "spdlog/spdlog.h"

#include "iterators.hpp"
#include "json_utils.hpp"
#include "lib_file.hpp"

LibFile::LibFile(const std::string &filename) : filename_(filename) { si2drPIInit(&err_); }

LibFile::~LibFile() {
  spdlog::info("Closing '{}' ...", filename_);
  si2drPIQuit(&err_);
}

/**
 * @brief Writes the JSON data stored in the lib_json_ member to a file.
 *
 * This function attempts to open the specified file for writing. If the file
 * cannot be opened, an error message is logged. If the file is successfully
 * opened, the JSON data is written to the file with an indentation of 2 spaces.
 * After writing, the file is closed and a success message is logged.
 *
 * @param filename The name of the file to which the JSON data will be written.
 */
void LibFile::writeJsonToFile(const std::string &filename) {
  std::ofstream out(filename);
  if (!out.is_open()) {
    spdlog::error("Could not open file '{}' for writing", filename);
    return;
  }
  out << lib_json_.dump(2);
  out.close();
  spdlog::info("JSON data written to '{}'", filename);
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
  spdlog::info("Reading '{}' ...", filename_);

  auto start = std::chrono::high_resolution_clock::now();
  si2drReadLibertyFile(const_cast<char *>(filename_.c_str()), &err_);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = end - start;

  if (err_ == SI2DR_INVALID_NAME) {
    spdlog::error("Could not open file '{}' for parsing, quitting...", filename_);
    exit(301);
  } else if (err_ == SI2DR_SYNTAX_ERROR) {
    spdlog::error("Syntax Errors were detected in the input file!");
    exit(401);
  } else {
    spdlog::info("Done. Read time: {:.2f} seconds", duration.count());
  }
}

/**
 * @brief Parses the library file and extracts relevant information.
 *
 * This function performs the following tasks:
 * - Logs the start of the parsing process.
 * - Iterates through the top-level groups in the library file.
 * - Extracts and logs the library name.
 * - Iterates through the second-level groups within each top-level group.
 * - Handles cells by generating JSON representations and storing them.
 * - Handles operating conditions by extracting PVT (Process, Voltage, Temperature) attributes and
 * storing them.
 *
 * The extracted information is stored in the `lib_json_` member variable.
 *
 * Logging is performed at various levels (info, warn, debug) to provide detailed information about
 * the parsing process.
 */
void LibFile::parse() {
  spdlog::info("Parsing '{}' ...", filename_);
  // Top level groups
  GroupsIterator group_iter(si2drPIGetGroups(&err_), err_);
  for (; !group_iter.end(); group_iter.next()) {
    LibGroup lib_group = group_iter.get();

    if (lib_group.getName() != "") {
      libname_ = lib_group.getName();
      spdlog::info("Library Name: {}", libname_);
    } else {
      spdlog::warn("Library Name: <NONAME>");
    }
    // Second level groups
    GroupsIterator sub_group_iter(lib_group.getGroups(), err_);
    for (; !sub_group_iter.end(); sub_group_iter.next()) {
      LibGroup lib_sub_group = sub_group_iter.get();

      std::string sub_group_type = lib_sub_group.getType();
      std::string sub_group_name = lib_sub_group.getName();

      if (sub_group_type == "cell") {
        spdlog::debug("Cell Name: {}", sub_group_name);
        // if (sub_group_name != "AN2D0") {
        //   continue;
        // }
        // Handle each cell
        json cell_json = generateCellJson(lib_sub_group, err_);
        lib_json_["cells"].push_back(cell_json);
      } else if (sub_group_type == "operating_conditions") {
        spdlog::info("Operating Conditions: {}", sub_group_name);
        // Get PVT from Operating Conditions
        AttributesIterator attr_iter(lib_sub_group.getAttrs(), err_);
        for (; !attr_iter.end(); attr_iter.next()) {
          LibAttribute lib_attr = attr_iter.get();
          spdlog::debug("Attribute Name: {}", lib_attr.getName());
          spdlog::debug("Int Value: {}", lib_attr.getInt());
          spdlog::debug("Float Value: {}", lib_attr.getFloat());
          spdlog::debug("String Value: {}", lib_attr.getString());
          if (lib_attr.getName() == "process") {
            process_ = lib_attr.getInt();
            lib_json_["process"] = process_;
          } else if (lib_attr.getName() == "voltage") {
            voltage_ = lib_attr.getFloat();
            lib_json_["voltage"] = voltage_;
          } else if (lib_attr.getName() == "temperature") {
            temperature_ = lib_attr.getInt();
            lib_json_["temperature"] = temperature_;
          }
        }
        spdlog::info("P: {}, V: {}, T: {}", process_, voltage_, temperature_);
      }
    }
  }
}

void LibFile::modify() { spdlog::info("Modifying the file..."); }

/**
 * @brief Checks the monotonicity of timing arc values in a given cell.
 *
 * This function verifies if the timing arc values in the provided JSON objects are monotonic.
 * It logs detailed information about the cell, pin, and related pin being checked, and reports
 * any non-monotonic values found.
 *
 * @param cell The JSON object representing the cell.
 * @param pin The JSON object representing the pin.
 * @param arc The JSON object representing the timing arc.
 * @param timing_arc_type The type of the timing arc to check.
 * @return true if the timing arc values are monotonic, false otherwise.
 *
 * The function performs the following steps:
 * 1. Logs the cell, pin, related pin, and timing arc type being checked.
 * 2. Assumes the values are monotonic initially.
 * 3. Checks if the timing arc contains the specified type and if it has a "values" array.
 * 4. Generates a matrix of values from the JSON array.
 * 5. Validates the format of the "values" array and logs errors for invalid formats.
 * 6. Checks for non-numeric values in the "values" array and logs warnings for such values.
 * 7. Checks the monotonic incrementality of rows in the matrix.
 * 8. Logs warnings for non-monotonic values, including the "when" key if present.
 * 9. Returns the monotonicity status.
 */
bool checkTimingArcMonotonicity(const json &cell, const json &pin, const json &arc,
                                const std::string &timing_arc_type) {
  spdlog::debug("Checking cell: '{}', pin: '{}', related_pin: '{}', timing_arc: '{}'",
                cell["cell_name"].get<std::string>(), pin["pin_name"].get<std::string>(),
                arc["related_pin"].get<std::string>(), timing_arc_type);
  bool is_monotonic = true; // Assume the values are monotonic initially
  if (arc.contains(timing_arc_type) && arc[timing_arc_type].contains("values")) {
    std::vector<std::vector<double>> value_matrix;
    // Generate a matrix of values from the JSON array
    for (const auto &row_val : arc[timing_arc_type]["values"]) {
      std::vector<double> row_data;
      // Check if the value is an array
      if (!row_val.is_array()) {
        spdlog::error("Invalid format: '{}' values should be an array of arrays in cell '{}', pin "
                      "'{}', related_pin '{}'",
                      timing_arc_type, cell["cell_name"].get<std::string>(),
                      pin["pin_name"].get<std::string>(), arc["related_pin"].get<std::string>());
        return false;
      }
      // Check for non-numeric values
      for (const auto &val : row_val) {
        if (val.is_number()) {
          row_data.push_back(val.get<double>());
        } else {
          spdlog::warn("Non-numeric value found in '{}'.values, skipping value: {} in cell '{}', "
                       "pin '{}', related_pin '{}'",
                       timing_arc_type, val.dump(), cell["cell_name"].get<std::string>(),
                       pin["pin_name"].get<std::string>(), arc["related_pin"].get<std::string>());
        }
      }
      value_matrix.push_back(row_data);
    }
    // Check the matrix for monotonicity
    if (!value_matrix.empty() && !value_matrix[0].empty()) {
      // Check the monotonic incrementality of rows
      for (size_t i = 0; i < value_matrix.size(); ++i) {
        for (size_t j = 1; j < value_matrix[i].size(); ++j) {
          if (value_matrix[i][j] <= value_matrix[i][j - 1]) {
            // If contains "when" key, log the when value
            if (arc.contains("when") && !arc["when"].get<std::string>().empty()) {
              spdlog::warn("Non-monotonic (by load) '{}' values: ({}, {}) {} < ({}, {}) {} "
                           "for Cell: {} Pin: {}->{} when: \"{}\"",
                           timing_arc_type, i, j, value_matrix[i][j], i, j - 1, value_matrix[i][j - 1],
                           cell["cell_name"].get<std::string>(), arc["related_pin"].get<std::string>(),
                           pin["pin_name"].get<std::string>(), arc["when"].get<std::string>());
            } else {
              spdlog::warn("Non-monotonic (by load) '{}' values: ({}, {}) {} < ({}, {}) {} "
                           "for Cell: {} Pin: {}->{}",
                           timing_arc_type, i, j, value_matrix[i][j], i, j - 1, value_matrix[i][j - 1],
                           cell["cell_name"].get<std::string>(), arc["related_pin"].get<std::string>(),
                           pin["pin_name"].get<std::string>());
            }
            is_monotonic = false;
          }
        }
      }
    } else {
      spdlog::warn("Empty or invalid 'values' array found for cell: '{}', pin: '{}', related_pin: '{}', "
                   "timing_arc: '{}'",
                   cell["cell_name"].get<std::string>(), pin["pin_name"].get<std::string>(),
                   arc["related_pin"].get<std::string>(), timing_arc_type);
    }
  }
  return is_monotonic;
}

void LibFile::mono() {
  /*
   * Use this command to check the following data in the current library to ensure
   * the tables are monotonically increasing with respect to output load:
   * cell_rise retaining_rise
   * cell_fall retaining_fall
   * rise_transition retain_rise_slew
   * fall_transition retain_fall_slew
   * mpw
   */
  spdlog::info("Monotonicity check of {}", filename_ + ".json");

  // Read the JSON file into json object
  std::ifstream in(filename_ + ".json");
  if (!in.is_open()) {
    spdlog::error("Could not open file '{}' for reading", filename_ + ".json");
    return;
  }
  json j = json::parse(in);
  in.close();

  std::map<std::string, bool> cell_monotonicity_status; // Track pass/fail status for each cell
  std::vector<std::string> failed_cells;                // List of failed cell names
  int total_cells = 0;
  int passed_cells = 0;

  // Check the monotonicity of delay values
  // The index 1 (time values) must be monotonically increasing and >= 0
  for (const auto &cell : j["cells"]) {
    total_cells++;
    bool cell_is_monotonic = true; // Assume cell is monotonic initially
    std::string cell_name = cell["cell_name"].get<std::string>();
    cell_monotonicity_status[cell_name] = true; // Initialize to pass, will be set to false if any check fails

    if (cell.contains("output_pins")) {
      for (const auto &pin : cell["output_pins"]) {
        if (pin.contains("timing_arcs")) {
          for (const auto &arc : pin["timing_arcs"]) {
            // Check 4 timing arc types and accumulate monotonicity status
            for (const auto &type : {"cell_rise", "cell_fall", "rise_transition", "fall_transition"}) {
              if (!checkTimingArcMonotonicity(cell, pin, arc, type)) {
                cell_is_monotonic = false;
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
  spdlog::info("Monotonicity check complete.");

  // Output summary statistics
  spdlog::info("validate_monotonicity : {} out of {} cells passed", passed_cells, total_cells);
  spdlog::info("validate_monotonicity : {} out of {} cells failed", total_cells - passed_cells, total_cells);
  if (!failed_cells.empty()) {
    std::stringstream failed_cells_ss;
    for (size_t i = 0; i < failed_cells.size(); ++i) {
      failed_cells_ss << failed_cells[i];
      if (i < failed_cells.size() - 1) {
        failed_cells_ss << ", "; // Add comma if not the last element
      }
    }
    spdlog::info("Failed cell list : {}", failed_cells_ss.str());
  }
}