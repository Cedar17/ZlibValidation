#ifndef LIB_FILE_H
#define LIB_FILE_H

#include <string>

#include "si2dr_liberty.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

class LibFile {
public:
  LibFile(const std::string &filename);
  ~LibFile();
  void read();
  void parse();
  void modify();
  void writeJsonToFile(const std::string &filename);

private:
  std::string filename_;
  si2drErrorT err_;
  std::string libname_;
  int process_;
  float voltage_;
  int temperature_;
  json lib_json_;
  json cells_json_;
};

#endif // LIB_FILE_H
