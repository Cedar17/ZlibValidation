#ifndef LOGIC_COMPARATOR_HPP
#define LOGIC_COMPARATOR_HPP

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <optional> // To store tables optionally
#include <regex>    // For regular expressions
#include <variant>

#include "exprtk.hpp"
#include "tabulate/markdown_exporter.hpp"
#include "tabulate/table.hpp"
#include <spdlog/spdlog.h>

#include "version.h"

using namespace tabulate;

// Structure to hold results for a single pin comparison
struct PinComparisonResult {
  std::string pin_name;
  std::string ref_expr_raw;
  std::string comp_expr_raw;
  std::string ref_expr_processed;
  std::string comp_expr_processed;
  bool comparison_possible = false; // Was comparison attempted?
  bool are_equivalent = false;
  bool ref_compiles = false;
  bool comp_compiles = false;
  std::optional<Table> ref_truth_table; // Store tables only if needed/successful
  std::optional<Table> comp_truth_table;
  std::string error_message; // Store any error during comparison
};

class LogicComparator {
public:
  LogicComparator(const std::map<std::string, std::string> &ref_outpin_map,
                  const std::map<std::string, std::string> &comp_outpin_map,
                  const std::string &cell_name);

  // Example code from exprtk documentation
  void logic();

  // Preprocessing function
  std::string preprocessExpression(const std::string &input_expr);

  // Helper to extract unique sorted variables from TWO expressions
  bool extractVariables(const std::string &expr1, const std::string &expr2,
                        std::vector<std::string> &sorted_vars);

  /**
   * @brief Compares two preprocessed logic expression strings for equivalence.
   *        Internal helper function.
   *
   * @tparam T The numeric type used by ExprTk (e.g., double, float).
   * @param ref_expression_processed The preprocessed reference logic expression.
   * @param comp_expression_processed The preprocessed comparison logic expression.
   * @param sorted_vars A sorted vector of unique variable names common to both expressions.
   * @param result Reference to a PinComparisonResult object to store detailed results.
   */
  void compareSingleExpressionPair(const std::string &ref_expression_processed,
                                   const std::string &comp_expression_processed,
                                   const std::vector<std::string> &sorted_vars,
                                   PinComparisonResult &result); // Pass result struct

  /**
   * @brief Compares logic for all output pins defined in the input maps,
   *        and stores results in all_pin_results_.
   */
  void compareCellLogic();

  /**
   * @brief Generates a comparison report file based on cell logic comparison results.
   *
   * @param output_file Path to the output report file.
   */
  void generateReport(const std::string &output_file);

private:
  std::map<std::string, std::string> ref_outpin_map_;
  std::map<std::string, std::string> comp_outpin_map_;
  std::string cell_name_;
  std::map<std::string, PinComparisonResult> all_pin_results_;
};

#endif // LOGIC_COMPARATOR_HPP