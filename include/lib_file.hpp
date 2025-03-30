#ifndef LIB_FILE_H
#define LIB_FILE_H

#include <filesystem>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "si2dr_liberty.h"

using json = nlohmann::json;

class LibFile {
public:
  LibFile(const std::string &filepath, const std::string &loggername);
  ~LibFile();
  std::shared_ptr<spdlog::logger> logger_;
  std::filesystem::path filepath_; // full path to the file
  std::string basename_;           // file name without extension
  std::string filename_;           // full file name with extension
  std::string libname_ = "";       // library name obtained through parsing
  std::string jsonname_ = "";      // json file name to store parsed data
  std::string loggername_ = "";    // log file name
  json lib_json_ = json::object();
  void writeJsonToFile();
  void parse();
  void modify();
  void mono(const bool is_slew);
  void supercell(const int chain_length, const std::vector<std::string> &cell_names);
  void verilog(const int chain_length, const std::vector<std::string> &cell_names);
  std::map<std::string, std::string> logic(const std::string &cell_name);

private:
  si2drErrorT err_;
  int process_ = 0;
  float voltage_ = 0.0;
  int temperature_ = 0;
  void read();
  bool checkTimingArcMonotonicity(const json &cell, const json &pin, const json &arc,
                                  const std::string &type, bool is_slew);
};

#endif // LIB_FILE_H