#ifndef JSON_UTILS_HPP
#define JSON_UTILS_HPP

#include <string>
#include <utility>

#include "nlohmann/json.hpp"
#include "si2dr_liberty.h"

#include "lib_group.hpp"

using json = nlohmann::json;

json generateLutJson(LibGroup &lib_lut_group, si2drErrorT &err);
json generatePowerJson(LibGroup &lib_power_group, si2drErrorT &err);
std::pair<std::string, json> generatePinJson(LibGroup &lib_pin_group, si2drErrorT &err);
json generateCellJson(LibGroup &lib_cell_group, si2drErrorT &err);

#endif // JSON_UTILS_HPP