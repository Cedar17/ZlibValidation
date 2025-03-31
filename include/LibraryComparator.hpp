#ifndef COMPARE_HPP
#define COMPARE_HPP

#include "LibFile.hpp"

#include "nlohmann/json.hpp"
#include "tabulate/table.hpp"

using json = nlohmann::json;
using namespace tabulate;

class LibraryComparator {
public:
  LibraryComparator(LibFile &ref_libfile, LibFile &comp_libfile, double reltol, double abstol);
  std::filesystem::path ref_lib_path_;
  std::filesystem::path comp_lib_path_;
  void generateReport(const std::string &output_file);

private:
  json ref_json_;
  json comp_json_;
  double reltol_;
  double abstol_;

  void compareCell(const std::string &cell_name, const json &ref_cell, const json &comp_cell,
                   Table &table);
  void comparePin(const std::string &cell_name, const std::string &pin_name, const json &ref_pin,
                  const json &comp_pin, Table &table);
  void compareTimingArc(const std::string &cell_name, const std::string &pin_name,
                        const std::string &timing_type, const json &ref_timing_arc,
                        const json &comp_timing_arc, Table &table);
  void compareLut(const std::string &cell_name, const std::string &pin_name,
                  const std::string &timing_type, const std::string &related_pin,
                  const std::string &arc_name, const json &ref_lut, const json &comp_lut, Table &table);
  // void configureTableFormat(Table &table);
};

#endif // COMPARE_HPP