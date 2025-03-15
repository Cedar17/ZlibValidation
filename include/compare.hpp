#ifndef COMPARE_HPP
#define COMPARE_HPP

#include "lib_file.hpp"

#include "nlohmann/json.hpp"
#include "tabulate/table.hpp"

using json = nlohmann::json;
using namespace tabulate;

class LibraryComparator {
public:
  LibraryComparator(LibFile &ref_libfile, LibFile &comp_libfile, double reltol);
  std::filesystem::path ref_lib_path_;
  std::filesystem::path comp_lib_path_;
  void generateReport(const std::string &output_file);

private:
  json ref_json;
  json comp_json;
  double reltol_;

  void compareCells(Table &report_table);
  void compareTimingArcs(const json &ref_cell, const json &comp_cell, Table &table,
                         const std::string &cell_name);
  void configureTableFormat(Table &table);
};

#endif // COMPARE_HPP