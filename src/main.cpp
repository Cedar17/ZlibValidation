// ./src/main.cpp
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include "CLI/CLI.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "si2dr_liberty.h"
#include "nlohmann/json.hpp"

#include "version.h"

void print_info() {
  spdlog::info("Version {}, Built: {}", APP_VERSION, BUILD_TIMESTAMP);
  spdlog::info("Author: {}, Email: {}", APP_AUTHOR, APP_CONTACT);
}

// Forward declarations
class LibAttribute;
class LibGroup;
class LibFile;



class LibFile {
public:
  std::string libname_;
  int process_;
  float voltage_;
  int temperature_;

  LibFile(const std::string &filename) : filename_(filename){
    si2drPIInit(&err_);
  }

  ~LibFile() {
    spdlog::info("Closing '{}' ...", filename_);
    si2drPIQuit(&err_);
  }

  void read() {
    spdlog::info("Reading '{}' ...", filename_);

    auto start = std::chrono::high_resolution_clock::now();
    si2drReadLibertyFile(const_cast<char *>(filename_.c_str()), &err_);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    if (err_ == SI2DR_INVALID_NAME) {
      spdlog::error("COULD NOT OPEN {} for parsing-- quitting...", filename_);
      exit(301);
    } else if (err_ == SI2DR_SYNTAX_ERROR) {
      spdlog::error("Syntax Errors were detected in the input file!");
      exit(401);
    } else {
      spdlog::info("Done. Read time: {:.2f} seconds", duration.count());
    }
  }

  void parse() {
    spdlog::info("Parsing the file...");

    si2drGroupsIdT groups;
    si2drGroupIdT group;
    groups = si2drPIGetGroups(&err_);
    while (!si2drObjectIsNull(group = si2drIterNextGroup(groups, &err_), &err_)) {
      si2drNamesIdT gnames;
      si2drStringT gname;

      gnames = si2drGroupGetNames(group, &err_);
      gname = si2drIterNextName(gnames, &err_);
      si2drIterQuit(gnames, &err_);

      if (gname) {
        libname_ = gname;
        spdlog::info("Library Name: {}", libname_);
      } else {
        spdlog::warn("Library Name: <NONAME>");
      }

      si2drGroupsIdT sub_groups;
      si2drGroupIdT sub_group;
      sub_groups = si2drGroupGetGroups(group, &err_);
      while (!si2drObjectIsNull(sub_group = si2drIterNextGroup(sub_groups, &err_), &err_)) {
        si2drStringT sub_group_type = si2drGroupGetGroupType(sub_group, &err_);
        si2drNamesIdT sub_group_names = si2drGroupGetNames(sub_group, &err_);
        si2drStringT sub_group_name = si2drIterNextName(sub_group_names, &err_);

        si2drIterQuit(sub_group_names, &err_);
        if (!strcmp(sub_group_type, "cell")) {
          spdlog::debug("Cell Name: {}", sub_group_name);
        }	else if (!strcmp(sub_group_type, "operating_conditions"))
        {
          spdlog::info("Operating Conditions: {}", sub_group_name);
          // print all the attrs
          si2drAttrsIdT attrs;
          si2drAttrIdT attr;
          attrs = si2drGroupGetAttrs(sub_group, &err_);
          for (attr = si2drIterNextAttr(attrs, &err_);
              !si2drObjectIsNull(attr, &err_);
              attr = si2drIterNextAttr(attrs, &err_))
          {
            si2drAttrTypeT at = si2drAttrGetAttrType(attr, &err_);
            si2drValuesIdT  vals;
            si2drInt32T     intgr= si2drSimpleAttrGetInt32Value(attr,&err_);
            si2drFloat64T   float64= si2drSimpleAttrGetFloat64Value(attr,&err_);
            si2drStringT    string, nam= si2drAttrGetName(attr,&err_);
            if (!strcmp(nam, "process"))
              process_ = intgr;
            if (!strcmp(nam, "temperature"))
              temperature_ = intgr;
            if (!strcmp(nam, "voltage"))
              voltage_ = float64;
          }
          si2drIterQuit(attrs, &err_);
          spdlog::info("Process: {}, Temperature: {}, Voltage: {}", process_, temperature_, voltage_);
          continue;
        }      
      }
      si2drIterQuit(sub_groups, &err_);
    }
    si2drIterQuit(groups, &err_);

  }

  void modify() {
    spdlog::info("Modifying the file...");
  }

private:
  std::string filename_;
  si2drErrorT err_;
};



/**
 * @brief Sets up the logger with both console and file sinks.
 *
 * This function initializes a logger with two sinks: one for console output
 * with color and another for file output. The console sink logs messages at
 * the info level and above, while the file sink logs messages at the debug
 * level and above. The logger itself is set to log messages at the debug level
 * and above. The logger is then set as the default logger for the application.
 */
void setup_logger() {
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  console_sink->set_level(spdlog::level::info);

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logfile.log", true);
  file_sink->set_level(spdlog::level::debug);

  std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};
  auto logger = std::make_shared<spdlog::logger>(APP_NAME, sinks.begin(), sinks.end());
  logger->set_level(spdlog::level::debug);
  spdlog::set_default_logger(logger);
}

int main(int argc, char *argv[]) {
  // Setup the global logger
  setup_logger();

  // Parse command line arguments
  std::string filename;
  std::string mode;

  // Command line parameter parsing using CLI11
  CLI::App app{APP_NAME};
  app.set_version_flag("-v,--version", APP_VERSION);
  app.add_option("-m,--mode", mode, "Choose the mode: 'parse' or 'modify'")
      ->default_val("parse")
      ->check(CLI::IsMember({"parse", "modify"}));
  app.add_option("-f,--file", filename, "Specify the file to process")
      ->check(CLI::ExistingFile)
      ->required();
  auto formatter = std::make_shared<CLI::Formatter>();
  formatter->column_width(45);
  app.formatter(formatter);
  CLI11_PARSE(app, argc, argv);

  print_info();

  // Begin reading the file
  LibFile libfile(filename);
  libfile.read();

  if (mode == "parse") {
    libfile.parse();
  } else if (mode == "modify") {
    libfile.modify();
  }

  spdlog::info("All Done!");
  return 0;
}