#include <chrono>
#include <fstream>

#include "spdlog/spdlog.h"

#include "iterators.hpp"
#include "json_utils.hpp"
#include "lib_file.hpp"


LibFile::LibFile(const std::string &filename) : filename_(filename) {
  si2drPIInit(&err_);
}

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
  spdlog::info("JSON data successfully written to '{}'", filename);
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
    spdlog::error("COULD NOT OPEN {} for parsing-- quitting...", filename_);
    exit(301);
  } else if (err_ == SI2DR_SYNTAX_ERROR) {
    spdlog::error("Syntax Errors were detected in the input file!");
    exit(401);
  } else {
    spdlog::info("Done. Read time: {:.2f} seconds", duration.count());
  }
}


void LibFile::parse() {
  spdlog::info("Parsing the file...");
  // Top level groups
  GroupsIterator group_iter(si2drPIGetGroups(&err_), err_);
  for (group_iter.begin(); !group_iter.end(); group_iter.next()) {
    LibGroup lib_group = group_iter.get();
  
    if (lib_group.getName() != "") {
      libname_ = lib_group.getName();
      spdlog::info("Library Name: {}", libname_);
    } else {
      spdlog::warn("Library Name: <NONAME>");
    }
    // Second level groups
    GroupsIterator sub_group_iter(lib_group.getGroups(), err_);
    for (sub_group_iter.begin(); !sub_group_iter.end(); sub_group_iter.next()) {
      LibGroup lib_sub_group = sub_group_iter.get();

      std::string sub_group_type = lib_sub_group.getType();
      std::string sub_group_name = lib_sub_group.getName();

      if (sub_group_type == "cell") {
        spdlog::debug("Cell Name: {}", sub_group_name);
        // Handle each cell
        json cell_json = generateCellJson(lib_sub_group, err_);
        lib_json_["cells"].push_back(cell_json);
      } else if (sub_group_type == "operating_conditions") {
        spdlog::info("Operating Conditions: {}", sub_group_name);
        // Get PVT from Operating Conditions
        AttributesIterator attr_iter(lib_sub_group.getAttrs(), err_);
        for (attr_iter.begin(); !attr_iter.end(); attr_iter.next()) {
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


void LibFile::modify() {
  spdlog::info("Modifying the file...");
}