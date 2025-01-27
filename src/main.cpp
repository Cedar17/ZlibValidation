#include "CLI/CLI.hpp"
#include "spdlog/spdlog.h"
#include "si2dr_liberty.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

void print_version() {
  spdlog::info("ZlibValidation version 0.1.0");
  spdlog::info("Author: Song Zixuan, 2025-01-26");
}

int main(int argc, char *argv[]) {
  CLI::App app{"ZlibValidation"};

  bool show_version = false;
  std::string filename;

  app.add_flag("-v,--version", show_version, "Show version information");
  app.add_option("--file", filename, "Specify the file to process")
      ->check(CLI::ExistingFile);

  CLI11_PARSE(app, argc, argv);

  if (show_version) {
    print_version();
    return 0;
  }

  // Begin reading the file
  print_version();
  spdlog::info("Reading '{}' ...", filename);

  si2drErrorT err;
  si2drPIInit(&err);

  auto start = std::chrono::high_resolution_clock::now();
  si2drReadLibertyFile(const_cast<char *>(filename.c_str()), &err);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = end - start;

  if (err == SI2DR_INVALID_NAME) {
    spdlog::error("COULD NOT OPEN {} for parsing-- quitting...", filename);
    return 301;
  } else if (err == SI2DR_SYNTAX_ERROR) {
    spdlog::error("Syntax Errors were detected in the input file!");
    return 401;
  } else {
    spdlog::info("Done. Read time: {:.2f} seconds", duration.count());
  }

  si2drGroupsIdT groups;
  si2drGroupIdT group;
  groups = si2drPIGetGroups(&err);
  while (!si2drObjectIsNull(group = si2drIterNextGroup(groups, &err), &err)) {
    si2drNamesIdT gnames;
    si2drStringT gname;

    gnames = si2drGroupGetNames(group, &err);
    gname = si2drIterNextName(gnames, &err);
    si2drIterQuit(gnames, &err);

    if (gname) {
      spdlog::info("Library Name: {}", gname);
    } else {
      spdlog::error("Library Name: <NONAME>");
      return 1;
    }
  }
  si2drIterQuit(groups, &err);

  spdlog::info("Quitting...");
  si2drPIQuit(&err);
  spdlog::info("Done!");
  return 0;
}