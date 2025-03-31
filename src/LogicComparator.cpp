#include "LogicComparator.hpp"

LogicComparator::LogicComparator(const std::map<std::string, std::string> &ref_outpin_map,
                                 const std::map<std::string, std::string> &comp_outpin_map,
                                 const std::string &cell_name)
    : ref_outpin_map_(ref_outpin_map), comp_outpin_map_(comp_outpin_map), cell_name_(cell_name) {}

void LogicComparator::generateReport(const std::string &output_file) {}