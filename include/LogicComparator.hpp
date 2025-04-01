#ifndef LOGIC_COMPARATOR_HPP
#define LOGIC_COMPARATOR_HPP

#include <map>
#include <string>

#include "exprtk.hpp"
#include "tabulate/table.hpp"

using namespace tabulate;

class LogicComparator {
public:
  LogicComparator(const std::map<std::string, std::string> &ref_outpin_map,
                  const std::map<std::string, std::string> &comp_outpin_map,
                  const std::string &cell_name);
  template <typename T> void logic();
  void generateReport(const std::string &output_file);

private:
  std::map<std::string, std::string> ref_outpin_map_;
  std::map<std::string, std::string> comp_outpin_map_;
  std::string cell_name_;
  Table table_;
};

#endif // LOGIC_COMPARATOR_HPP