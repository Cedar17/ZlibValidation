#include "spdlog/spdlog.h"

#include "iterators.hpp"
#include "json_utils.hpp"

/*
json 类型的值可以是以下几种类型之一：
方括号 []：用于包含一组有序的值（数组）。
数组中的每个值可以是任意类型的 JSON 值，包括对象、数组、字符串、数字、布尔值或 null。
花括号 {}：用于包含一组键值对（对象）。
对象中的每个键都是一个字符串，值可以是任意类型的 JSON 值，包括对象、数组、字符串、数字、布尔值或
null。
*/

/**
 * @brief Parses a comma-separated string into a vector of doubles.
 *
 * This function takes a string containing comma-separated values and converts
 * it into a vector of doubles. Each value in the string is expected to be a
 * valid representation of a double.
 *
 * @param str The input string containing comma-separated double values.
 * @return A vector of doubles parsed from the input string.
 */
std::vector<double> parseStringToVector(const std::string &str) {
  std::vector<double> result;
  std::stringstream ss(str);
  std::string item;
  while (std::getline(ss, item, ',')) {
    result.push_back(std::stod(item));
  }
  return result;
}

/**
 * @brief Generates a JSON object representing a lookup table (LUT) from a given library group.
 *
 * This function iterates over the attributes of the provided library group and processes
 * them to generate a JSON object. It specifically handles attributes named "index_1",
 * "index_2", and "values", converting their string representations into vectors and
 * adding them to the JSON object. If an attribute with an unknown name is encountered,
 * a warning is logged.
 *
 * @param lib_lut_group The library group containing the LUT attributes.
 * @param err Reference to an error variable to capture any errors during attribute iteration.
 * @return A JSON object representing the LUT.
 */
json generateLutJson(LibGroup &lib_lut_group, si2drErrorT &err) {
  json lut_json;

  AttributesIterator attr_iter(lib_lut_group.getAttrs(), err);
  for (; !attr_iter.end(); attr_iter.next()) {
    LibAttribute lib_attr = attr_iter.get();
    std::string lut_attr_name = lib_attr.getName();

    // spdlog::debug("LUT Attribute Name: {}", lib_attr.getName());
    // spdlog::debug("Is Complex? {}", lib_attr.isComplex());

    if (lut_attr_name == "index_1" || lut_attr_name == "index_2") {
      ValuesIterator values_iter(lib_attr.getValues(), err);
      for (; !values_iter.end(); values_iter.next()) {
        // spdlog::debug("Type: {}", int(values_iter.vtype_)); // 5 is string
        // spdlog::debug("Str: {}", values_iter.str_);
        lut_json[lut_attr_name] = parseStringToVector(std::string(values_iter.str_));
      }
    } else if (lut_attr_name == "values") {
      ValuesIterator values_iter(lib_attr.getValues(), err);
      for (; !values_iter.end(); values_iter.next()) {
        lut_json["values"].push_back(parseStringToVector(std::string(values_iter.str_)));
      }
    } else {
      spdlog::warn("Unknown LUT attribute name: {}", lut_attr_name);
    }
  }
  return lut_json;
}

json generateTimingJson(LibGroup &lib_timing_group, si2drErrorT &err) {
  json timing_json;

  AttributesIterator attr_iter(lib_timing_group.getAttrs(), err);
  for (; !attr_iter.end(); attr_iter.next()) {
    LibAttribute lib_attr = attr_iter.get();

    std::string attr_name = lib_attr.getName();
    if (attr_name == "related_pin" || attr_name == "timing_type" || attr_name == "timing_sense" ||
        attr_name == "when") {
      timing_json[attr_name] = lib_attr.getString();
    }
    // More timing attributes can be added here
  }

  GroupsIterator timing_sub_group_iter(lib_timing_group.getGroups(), err);
  for (; !timing_sub_group_iter.end(); timing_sub_group_iter.next()) {
    LibGroup lib_timing_sub_group = timing_sub_group_iter.get();

    std::string timing_sub_group_type = lib_timing_sub_group.getType();
    // std::string timing_sub_group_name = lib_timing_sub_group.getName();
    if (timing_sub_group_type == "cell_fall" || timing_sub_group_type == "cell_rise" ||
        timing_sub_group_type == "fall_transition" || timing_sub_group_type == "rise_transition" ||
        timing_sub_group_type == "fall_constraint" || timing_sub_group_type == "rise_constraint") {
      timing_json[timing_sub_group_type] = generateLutJson(lib_timing_sub_group, err);
    } else {
      spdlog::warn("Unknown timing sub group type: {}", timing_sub_group_type);
    }
  }
  return timing_json;
}

json generatePowerJson(LibGroup &lib_power_group, si2drErrorT &err) {
  json power_json;

  AttributesIterator attr_iter(lib_power_group.getAttrs(), err);
  for (; !attr_iter.end(); attr_iter.next()) {
    LibAttribute lib_attr = attr_iter.get();

    std::string attr_name = lib_attr.getName();
    if (attr_name == "when" || attr_name == "related_pin" || attr_name == "related_pg_pin") {
      power_json[attr_name] = lib_attr.getString();
    }
    // More power attributes can be added here
  }

  GroupsIterator power_sub_group_iter(lib_power_group.getGroups(), err);
  for (; !power_sub_group_iter.end(); power_sub_group_iter.next()) {
    LibGroup lib_power_sub_group = power_sub_group_iter.get();

    std::string power_sub_group_type = lib_power_sub_group.getType();
    // std::string power_sub_group_name = lib_power_sub_group.getName();
    if (power_sub_group_type == "rise_power") {
      // spdlog::debug("Rise Power: {}", power_sub_group_name);
      power_json["rise_power"] = generateLutJson(lib_power_sub_group, err);
    } else if (power_sub_group_type == "fall_power") {
      power_json["fall_power"] = generateLutJson(lib_power_sub_group, err);
    } else {
      spdlog::warn("Unknown power sub group type: {}", power_sub_group_type);
    }
  }
  return power_json;
}

std::pair<std::string, json> generatePinJson(LibGroup &lib_pin_group, si2drErrorT &err) {
  json pin_json;
  std::string direction;
  pin_json["pin_name"] = lib_pin_group.getName();

  AttributesIterator attr_iter(lib_pin_group.getAttrs(), err);
  for (; !attr_iter.end(); attr_iter.next()) {
    LibAttribute lib_attr = attr_iter.get();

    std::string attr_name = lib_attr.getName();
    if (attr_name == "direction") {
      direction = lib_attr.getString();
    } else if (attr_name == "max_transition" || attr_name == "capacitance" ||
               attr_name == "rise_capacitance" || attr_name == "fall_capacitance" ||
               attr_name == "max_capacitance") {
      pin_json[attr_name] = lib_attr.getFloat();
    } else if (attr_name == "function" || attr_name == "power_down_function" ||
               attr_name == "related_ground_pin" || attr_name == "related_power_pin" ||
               attr_name == "three_state") {
      pin_json[attr_name] = lib_attr.getString();
    }
    // More pin attributes can be added here
  }

  GroupsIterator pin_sub_group_iter(lib_pin_group.getGroups(), err);
  for (; !pin_sub_group_iter.end(); pin_sub_group_iter.next()) {
    LibGroup lib_pin_sub_group = pin_sub_group_iter.get();

    std::string pin_sub_group_type = lib_pin_sub_group.getType();
    // std::string pin_sub_group_name = lib_pin_sub_group.getName();
    if (pin_sub_group_type == "internal_power") {
      json power_json = generatePowerJson(lib_pin_sub_group, err);
      pin_json["power_arcs"].push_back(power_json);
    } else if (pin_sub_group_type == "timing") {
      // spdlog::debug("Has Timing: {}", lib_pin_group.getName());
      json timing_json = generateTimingJson(lib_pin_sub_group, err);
      pin_json["timing_arcs"].push_back(timing_json);
    } else {
      spdlog::warn("Unknown pin sub group type: {}", pin_sub_group_type);
    }
  }
  return std::make_pair(direction, pin_json);
}

json generateCellJson(LibGroup &lib_cell_group, si2drErrorT &err) {
  json cell_json;
  cell_json["cell_name"] = lib_cell_group.getName();

  AttributesIterator attr_iter(lib_cell_group.getAttrs(), err);
  for (; !attr_iter.end(); attr_iter.next()) {
    LibAttribute lib_attr = attr_iter.get();

    std::string attr_name = lib_attr.getName();
    if (attr_name == "area" || attr_name == "cell_leakage_power") {
      cell_json[attr_name] = lib_attr.getFloat();
    } else if (attr_name == "cell_footprint") {
      cell_json[attr_name] = lib_attr.getString();
    }
    // More cell attributes can be added here
  }

  GroupsIterator cell_sub_group_iter(lib_cell_group.getGroups(), err);
  for (; !cell_sub_group_iter.end(); cell_sub_group_iter.next()) {
    LibGroup lib_cell_sub_group = cell_sub_group_iter.get();

    std::string cell_sub_group_type = lib_cell_sub_group.getType();
    // std::string cell_sub_group_name = lib_cell_sub_group.getName();

    // spdlog::debug("Cell Sub Group Type: {}", cell_sub_group_type);
    // spdlog::debug("Cell Sub Group Name: {}", cell_sub_group_name);

    if (cell_sub_group_type == "pin") {
      auto [direction, pin_json] = generatePinJson(lib_cell_sub_group, err);
      if (direction == "input") {
        cell_json["input_pins"].push_back(pin_json);
      } else if (direction == "output") {
        cell_json["output_pins"].push_back(pin_json);
      } else if (direction == "internal") {
        cell_json["internal_pins"].push_back(pin_json);
      } else if (direction == "inout") {
        cell_json["inout_pins"].push_back(pin_json);
      } else {
        spdlog::warn("Unknown direction: {}", direction);
      }
    }
  }
  return cell_json;
}