#ifndef LIB_FILE_H
#define LIB_FILE_H

#include <string>

#include "nlohmann/json.hpp"
#include "si2dr_liberty.h"

using json = nlohmann::json;

class LibFile {
public:
  LibFile(const std::string &filename);
  ~LibFile();
  void parse();
  void modify();
  void mono(const std::string json_file_name, const bool is_slew);
  void writeJsonToFile(const std::string &filename);
  void supercell(const std::string json_file_name, const int chain_length);
  
  private:
  si2drErrorT err_;
  std::string filename_;
  std::string libname_ = "";
  int process_ = 0;
  float voltage_ = 0.0;
  int temperature_ = 0;
  json lib_json_ = json::object();
  void read();
};

#endif // LIB_FILE_H
