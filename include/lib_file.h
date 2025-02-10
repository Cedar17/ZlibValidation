#ifndef LIB_FILE_H
#define LIB_FILE_H

#include <string>
#include "si2dr_liberty.h"

class LibFile {
public:
  LibFile(const std::string &filename);
  ~LibFile();
  void read();
  void parse();
  void modify();

private:
  std::string filename_;
  si2drErrorT err_;
  std::string libname_;
  int process_;
  float voltage_;
  int temperature_;
};

#endif // LIB_FILE_H