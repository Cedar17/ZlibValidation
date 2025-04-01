#ifndef LIBFILEOPERATIONS_H
#define LIBFILEOPERATIONS_H

#include <filesystem>
#include <iostream>
#include <thread>

#include "si2dr_liberty.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "LibFile.hpp"
#include "LibraryComparator.hpp"
#include "LogicComparator.hpp"
#include "LogicExtractor.hpp"

void printInfo();
void parseLibFile(const std::string &library_path, const std::string log_file_name);
void monoCheckLibFile(const std::string &library_path, const std::string log_file_name,
                      bool is_slew);
void supercellLibFile(const std::string &library_path, const std::string &log_file_name,
                      int chain_length, const std::vector<std::string> &cell_names);
void verilogLibFile(const std::string &library_path, const std::string &log_file_name,
                    int chain_length, const std::vector<std::string> &cell_names);
void compareLibFiles(const std::string &ref_lib, const std::string &comp_lib, const double reltol,
                     const double abstol, std::string &report_file_name);
void funcLibFile(const std::string &ref_file, const std::string &comp_file,
                 const std::vector<std::string> &cell_names, std::string &report_file_name);

#endif // LIBFILEOPERATIONS_H