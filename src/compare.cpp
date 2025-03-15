#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "spdlog/spdlog.h"
#include <tabulate/markdown_exporter.hpp>

#include "compare.hpp"
#include "version.h"

LibraryComparator::LibraryComparator(LibFile &ref_libfile, LibFile &comp_libfile, double reltol)
    : reltol_(reltol) {
  std::string ref_json_name = ref_libfile.basename_ + ".json";
  if (!std::filesystem::exists(ref_json_name)) {
    spdlog::info("Reference JSON file {} not found. Parsing first.", ref_json_name);
    ref_libfile.parse();
    ref_libfile.writeJsonToFile();
    ref_json = ref_libfile.lib_json_;
  }
  std::string comp_json_name = comp_libfile.basename_ + ".json";
  if (!std::filesystem::exists(comp_json_name)) {
    spdlog::info("Comparison JSON file {} not found. Parsing first.", comp_json_name);
    comp_libfile.parse();
    comp_libfile.writeJsonToFile();
    comp_json = comp_libfile.lib_json_;
  }
  std::ifstream comp_in(comp_json_name);

  if (ref_json.empty()) {
    std::ifstream ref_in(ref_json_name);
    if (!ref_in.is_open()) {
      spdlog::error("Could not open file '{}' for reading", ref_json_name);
      return;
    }
    try {
      ref_json = json::parse(ref_in);
    } catch (const json::parse_error &e) {
      spdlog::error("Error parsing reference JSON file '{}': {}", ref_json_name, e.what());
      return;
    }
  }
  if (comp_json.empty()) {
    if (!comp_in.is_open()) {
      spdlog::error("Could not open file '{}' for reading", comp_json_name);
      return;
    }
    try {
      comp_json = json::parse(comp_in);
    } catch (const json::parse_error &e) {
      spdlog::error("Error parsing comparison JSON file '{}': {}", comp_json_name, e.what());
      return;
    }
  }
  ref_lib_path_ = ref_libfile.filepath_;
  comp_lib_path_ = comp_libfile.filepath_;
  spdlog::info("Successfully loaded JSON files for comparison");
}

void LibraryComparator::compareCells(tabulate::Table &report_table) {
  auto ref_cells = ref_json["cells"];
  auto comp_cells = comp_json["cells"];

  for (auto &ref_cell : ref_cells) {
    auto cell_name = ref_cell["cell_name"].get<std::string>();
    auto comp_cell_it = std::find_if(comp_cells.begin(), comp_cells.end(), [&cell_name](const json &cell) {
      return cell["cell_name"] == cell_name;
    });

    if (comp_cell_it == comp_cells.end()) {
      report_table.add_row({"Missing cell", cell_name, "", "", "", "", "<"});
      continue;
    }

    compareTimingArcs(ref_cell, *comp_cell_it, report_table, cell_name);
  }
}

void LibraryComparator::compareTimingArcs(const json &ref_cell, const json &comp_cell, tabulate::Table &table,
                                          const std::string &cell_name) {
  auto get_timing_arcs = [](const json &cell) {
    std::vector<json> arcs;
    for (auto &pin : cell["output_pins"]) {
      for (auto &arc : pin["timing_arcs"]) {
        arcs.push_back(arc);
      }
    }
    return arcs;
  };

  auto ref_arcs = get_timing_arcs(ref_cell);
  auto comp_arcs = get_timing_arcs(comp_cell);

  for (size_t i = 0; i < ref_arcs.size(); ++i) {
    auto &ref_arc = ref_arcs[i];
    auto &comp_arc = comp_arcs[i];

    double ref_val = ref_arc["cell_rise"]["values"][0][0].get<double>();
    double comp_val = comp_arc["cell_rise"]["values"][0][0].get<double>();
    double diff = comp_val - ref_val;
    double diff_percent = (diff / ref_val) * 100;


    // table.add_row({
    //     cell_name,
    //     ref_arc["related_pin"],
    //     ref_val,
    //     comp_val,
    //     diff,
    //     diff_percent,
    //     is_outlier ? "<" : ""
    // });
  }
}

void LibraryComparator::generateReport(const std::string &output_file) {
  std::ofstream outfile(output_file);
  outfile << "# LIBRARY comparison\n" << std::endl;
  outfile << "**Reference library: " << ref_lib_path_ << "**\n" << std::endl;
  outfile << "**Comparison library: " << comp_lib_path_ << "**\n" << std::endl;
  outfile << "**Relative tolerance: " << reltol_ << "**\n" << std::endl;
  outfile << "**Performed by " << APP_NAME << " v" << APP_VERSION << " from " << APP_AUTHOR;
  auto now = std::chrono::system_clock::now();
  std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
  std::tm* now_tm = std::localtime(&now_time_t);
  outfile << ". on: " << std::put_time(now_tm, "%c") << "**\n" << std::endl;
  outfile << "> Legend: < outlier,  * scaled, ! indices switched, ^ slews extrapolated, ~ loads extrapolated, + padding added, " << std::endl;
  outfile << "> " << std::endl;
  outfile << "> Legend: /0 divide by zero, / slews interpolated, # loads interpolated" << std::endl;
  outfile << "> " << std::endl;
  outfile << "> Legend: << value is less but unknown, >> value is more but unknown\n" << std::endl ;

  tabulate::Table report;
  report.add_row({"Cell Name", "Pin", "Reference", "Compare", "Diff", "Diff%", "Outlier"});
  report.add_row({"1", "2", "3", "4", "5", "6", "7"});

  // compareCells(report);
  // center align 'Director' column
  report.column(2).format().font_align(FontAlign::center);

  // right align 'Estimated Budget' column
  report.column(3).format().font_align(FontAlign::right);

  // right align 'Release Date' column
  report.column(4).format().font_align(FontAlign::right);

  // Color header cells
  for (size_t i = 0; i < 5; ++i) {
    report[0][i].format().font_color(Color::yellow).font_style({FontStyle::bold});
  }

  // Check if the output file is Markdown
  if (output_file.substr(output_file.size() - 3) == ".md") {
    // Export to Markdown
    MarkdownExporter exporter;
    auto markdown = exporter.dump(report);
    outfile << markdown << std::endl;
  } else {
    // Export to file
    outfile << report << std::endl;
  }
  // Export to console
  std::cout << report << std::endl;
  
  outfile.close();
}