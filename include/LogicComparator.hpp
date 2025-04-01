#ifndef LOGIC_COMPARATOR_HPP
#define LOGIC_COMPARATOR_HPP

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional> // To store tables optionally
#include <regex>    // For regular expressions
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "exprtk.hpp"
#include "tabulate/table.hpp"
#include <spdlog/spdlog.h>

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
  template <typename T> void logic();

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
   * @param result Reference to a PinComparisonResult object to store detailed results.
   */
  template <typename T>
  void compareSingleExpressionPair(const std::string &ref_expression_processed,
                                   const std::string &comp_expression_processed,
                                   PinComparisonResult &result); // Pass result struct

  /**
   * @brief Compares logic for all output pins defined in the input maps.
   *
   * @tparam T Numeric type for ExprTk (e.g., double).
   * @return A map where the key is the pin name and the value is the
   *         PinComparisonResult struct containing comparison details.
   */
  template <typename T> std::map<std::string, PinComparisonResult> compareCellLogic();

  /**
   * @brief Generates a comparison report file based on cell logic comparison results.
   *
   * @param output_file Path to the output report file.
   * @param comparison_results The results obtained from compareCellLogic.
   */
  void generateReport(const std::string &output_file,
                      const std::map<std::string, PinComparisonResult> &comparison_results);

private:
  std::map<std::string, std::string> ref_outpin_map_;
  std::map<std::string, std::string> comp_outpin_map_;
  std::string cell_name_;
};

#endif // LOGIC_COMPARATOR_HPP