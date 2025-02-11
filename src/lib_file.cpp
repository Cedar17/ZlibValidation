#include <chrono>
#include <fstream>

#include "spdlog/spdlog.h"

#include "lib_file.hpp"
#include "iterators.hpp"


LibFile::LibFile(const std::string &filename) : filename_(filename) {
  si2drPIInit(&err_);
}

LibFile::~LibFile() {
  spdlog::info("Closing '{}' ...", filename_);
  si2drPIQuit(&err_);
}


/*
方括号 []：用于包含一组有序的值（数组）。
数组中的每个值可以是任意类型的 JSON 值，包括对象、数组、字符串、数字、布尔值或 null。
花括号 {}：用于包含一组键值对（对象）。
对象中的每个键都是一个字符串，值可以是任意类型的 JSON 值，包括对象、数组、字符串、数字、布尔值或 null。
*/
json generateCellJson(LibGroup &cell_group, si2drErrorT &err) {
  json cell_json;
  si2drNamesIdT sub_group_names = cell_group.getNames();
  si2drStringT sub_group_name = si2drIterNextName(sub_group_names, &err);
  si2drIterQuit(sub_group_names, &err);

  cell_json["cell_name"] = sub_group_name;

  si2drAttrsIdT attrs = cell_group.getAttrs();
  AttributesIterator attr_iter(attrs, err);
  for (attr_iter.begin(); !attr_iter.end(); attr_iter.next()) {
    LibAttribute libattr = attr_iter.get();
    cell_json["attributes"][libattr.getName()] = {
      // {"int", libattr.getInt()},
      {"float", libattr.getFloat()},
      {"string", libattr.getString()}
    };
  }
  return cell_json;
}


/**
 * @brief Writes the JSON representation of the library data to a file.
 *
 * This function serializes the internal JSON object, which includes
 * process, temperature, and voltage data, and writes it to the specified
 * file. If the file cannot be opened for writing, an error message is logged.
 * Upon successful writing, an informational message is logged.
 *
 * @param filename The name of the file to which the JSON data will be written.
 */
void LibFile::writeJsonToFile(const std::string &filename) {
  lib_json_["process"] = process_;
  lib_json_["temperature"] = temperature_;
  lib_json_["voltage"] = voltage_;

  std::ofstream out(filename);
  if (!out.is_open()) {
    spdlog::error("Could not open file '{}' for writing", filename);
    return;
  }
  out << lib_json_.dump(2);
  out.close();
  spdlog::info("JSON data successfully written to '{}'", filename);
}


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
  si2drGroupsIdT groups = si2drPIGetGroups(&err_);
  GroupsIterator group_iterator(groups, err_);
  for (group_iterator.begin(); !group_iterator.end(); group_iterator.next()) {
    LibGroup lib_group = group_iterator.get();
  
    si2drNamesIdT gnames = lib_group.getNames();
    si2drStringT gname = si2drIterNextName(gnames, &err_);
    si2drIterQuit(gnames, &err_);

    if (gname) {
      libname_ = gname;
      spdlog::info("Library Name: {}", libname_);
    } else {
      spdlog::info("Library Name: <NONAME>");
    }
    // Second level groups
    si2drGroupsIdT sub_groups = lib_group.getGroups();
    GroupsIterator sub_group_iterator(sub_groups, err_);
    for (sub_group_iterator.begin(); !sub_group_iterator.end(); sub_group_iterator.next()) {
      LibGroup lib_sub_group = sub_group_iterator.get();

      std::string sub_group_type = lib_sub_group.getType();
      si2drNamesIdT sub_group_names = lib_sub_group.getNames();
      si2drStringT sub_group_name = si2drIterNextName(sub_group_names, &err_);
      si2drIterQuit(sub_group_names, &err_);

      if (sub_group_type == "cell") {
        spdlog::debug("Cell Name: {}", sub_group_name);
        // handle cell
        json cell_json = generateCellJson(lib_sub_group, err_);
        lib_json_["cells"].push_back(cell_json);
      } else if (sub_group_type == "operating_conditions") {
        spdlog::info("Operating Conditions: {}", sub_group_name);
        // print all the attrs
        si2drAttrsIdT attrs = lib_sub_group.getAttrs();
        AttributesIterator attr_iter(attrs, err_);
        for (attr_iter.begin(); !attr_iter.end(); attr_iter.next()) {
          LibAttribute libattr = attr_iter.get();
          spdlog::debug("Attribute Name: {}", libattr.getName());
          spdlog::debug("Int Value: {}", libattr.getInt());
          spdlog::debug("Float Value: {}", libattr.getFloat());
          spdlog::debug("String Value: {}", libattr.getString());
          if (libattr.getName() == "process") {
            process_ = libattr.getInt();
          } else if (libattr.getName() == "temperature") {
            temperature_ = libattr.getInt();
          } else if (libattr.getName() == "voltage") {
            voltage_ = libattr.getFloat();
          }
        }
        spdlog::info("Process: {}, Temperature: {}, Voltage: {}", process_, temperature_, voltage_);
      }
    }
  }
}


void LibFile::modify() {
  spdlog::info("Modifying the file...");
}