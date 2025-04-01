#include "LogicComparator.hpp"

LogicComparator::LogicComparator(const std::map<std::string, std::string> &ref_outpin_map,
                                 const std::map<std::string, std::string> &comp_outpin_map,
                                 const std::string &cell_name)
    : ref_outpin_map_(ref_outpin_map), comp_outpin_map_(comp_outpin_map), cell_name_(cell_name) {}
template <typename T> void LogicComparator::logic() {
  typedef exprtk::symbol_table<T> symbol_table_t;
  typedef exprtk::expression<T> expression_t;
  typedef exprtk::parser<T> parser_t;

  const std::string expression_string = "not(A and B) or C";

  symbol_table_t symbol_table;
  symbol_table.create_variable("A");
  symbol_table.create_variable("B");
  symbol_table.create_variable("C");

  expression_t expression;
  expression.register_symbol_table(symbol_table);

  parser_t parser;
  parser.compile(expression_string, expression);

  printf(" # | A | B | C | %s\n"
         "---+---+---+---+-%s\n",
         expression_string.c_str(), std::string(expression_string.size(), '-').c_str());

  for (int i = 0; i < 8; ++i) {
    symbol_table.get_variable("A")->ref() = T(i & 0x01 ? 1 : 0);
    symbol_table.get_variable("B")->ref() = T(i & 0x02 ? 1 : 0);
    symbol_table.get_variable("C")->ref() = T(i & 0x04 ? 1 : 0);

    const int result = static_cast<int>(expression.value());

    printf(" %d | %d | %d | %d | %d \n", i,
           static_cast<int>(symbol_table.get_variable("A")->value()),
           static_cast<int>(symbol_table.get_variable("B")->value()),
           static_cast<int>(symbol_table.get_variable("C")->value()), result);
  }
}
template void LogicComparator::logic<double>();

// --- Preprocessing Function ---

/**
 * @brief Checks if a given string is a valid identifier.
 *
 * A valid identifier must:
 * - Not be empty.
 * - Consist of alphanumeric characters and underscores.
 * - Start with an uppercase alphabetic character.
 * - Contain at least one alphabetic character.
 *
 * @param token The string to check.
 * @return True if the string is a valid identifier, false otherwise.
 */
bool isIdentifier(const std::string &token) {
  if (token.empty())
    return false;
  // Check if the token is a valid identifier (upper alpha+ numeric + underscore)
  bool has_alpha = false;
  for (char c : token) {
    if (std::isalpha(c)) {
      has_alpha = true;
    } else if (!std::isalnum(c) && c != '_') {
      return false; // Contains invalid char
    }
  }
  // Must start with an uppercase alphabetic character
  return has_alpha && std::isupper(token[0]);
}

/**
 * @brief Checks if a given token is a logical operator.
 *
 * This function determines whether the input string `token` is one of the
 * supported logical operators: "and", "or", "xor", or "not".
 *
 * @param token The string to check.
 * @return True if the token is a logical operator, false otherwise.
 */
bool isOperator(const std::string &token) {
  static const std::set<std::string> operators = {"and", "or", "xor", "not"};
  return operators.count(token);
}

std::string LogicComparator::preprocessExpression(const std::string &input_expr) {
  std::string processed = input_expr;
  spdlog::debug("Preprocessing raw expression: {}", processed);

  // 0. Trim leading/trailing whitespace first
  processed = std::regex_replace(processed, std::regex("^\\s+|\\s+$"), "");

  // 1. Make replacements more robust by adding spaces first, then replacing
  // Add spaces around operators and parentheses that need them
  processed = std::regex_replace(processed, std::regex("\\("), " ( ");
  processed = std::regex_replace(processed, std::regex("\\)"), " ) ");
  processed = std::regex_replace(processed, std::regex("\\+"), " + ");
  processed = std::regex_replace(processed, std::regex("\\^"), " ^ ");
  processed = std::regex_replace(processed, std::regex("\\*"), " * ");
  // Handle '!' carefully, might be part of !=
  // Let's assume '!' is only used for NOT for now
  processed =
      std::regex_replace(processed, std::regex("!([^=])"), " not $1");  // \! followed by non-=
  processed = std::regex_replace(processed, std::regex("^!"), " not "); // \! at the beginning

  // 2. Replace logical operators with their equivalents(with spaces around them)
  processed = std::regex_replace(processed, std::regex(" \\+ "), " or ");
  processed = std::regex_replace(processed, std::regex(" \\^ "), " xor ");
  processed = std::regex_replace(processed, std::regex(" \\* "), " and ");

  spdlog::debug("Processed expression after replacements: {}", processed);

  // 3. Tokenize based on spaces
  std::stringstream ss(processed);
  std::istream_iterator<std::string> begin(ss);
  std::istream_iterator<std::string> end;
  std::vector<std::string> tokens(begin, end);

  if (tokens.empty()) {
    spdlog::debug("No tokens found after preprocessing.");
    return "";
  }

  // 4. Insert implied 'and'
  std::vector<std::string> processed_tokens;
  processed_tokens.push_back(tokens[0]); // 先添加第一个 token

  for (size_t i = 0; i < tokens.size() - 1; ++i) {
    const std::string &current_token = tokens[i];
    const std::string &next_token = tokens[i + 1];

    // 定义可以构成隐式 AND 的结尾和开头 token 类型
    bool current_ends_operand = isIdentifier(current_token) || current_token == ")";
    bool next_starts_operand = isIdentifier(next_token) || next_token == "(";

    // 定义不能插入 AND 的情况：当前或下一个是显式操作符，或括号组合不当
    bool current_is_invalid_before_and = isOperator(current_token) || current_token == "(";
    bool next_is_invalid_after_and = isOperator(next_token) || next_token == ")";

    // 检查是否应该插入 AND
    if (current_ends_operand && next_starts_operand && !current_is_invalid_before_and &&
        !next_is_invalid_after_and) {
      processed_tokens.push_back("and");
    }

    processed_tokens.push_back(next_token); // 添加下一个 token
  }

  // 5. Reconstruct the string
  std::string final_expr;
  for (size_t i = 0; i < processed_tokens.size(); ++i) {
    final_expr += processed_tokens[i] + (i == processed_tokens.size() - 1 ? "" : " ");
  }

  // 6. Final cleanup of spaces, especially around parentheses
  final_expr = std::regex_replace(final_expr, std::regex("\\s+"), " ");
  final_expr = std::regex_replace(final_expr, std::regex(" \\( "), "(");
  final_expr = std::regex_replace(final_expr, std::regex("\\( "), "(");
  final_expr = std::regex_replace(final_expr, std::regex(" \\("), "(");
  final_expr = std::regex_replace(final_expr, std::regex(" \\) "), ")");
  final_expr = std::regex_replace(final_expr, std::regex("\\) "), ")");
  final_expr = std::regex_replace(final_expr, std::regex(" \\)"), ")");
  final_expr = std::regex_replace(final_expr, std::regex("^\\s+|\\s+$"), "");

  spdlog::debug("Preprocessed expression result: {}", final_expr);
  return final_expr;
}

// --- Variable Extraction Helper ---
/**
 * @brief Extracts and validates variables from two expressions, ensuring they match.
 *
 * This function parses two raw expression strings (`expr1_raw` and `expr2_raw`) to identify
 * potential variable names. It uses a regular expression to find identifiers and then
 * validates them against a set of rules:
 *   1. The identifier must be a valid identifier as determined by the `isIdentifier` function.
 *   2. The identifier must not be a keyword or function name defined in the `keywords` set.
 *
 * The function compares the sets of validated variables from both expressions. If the sets
 * are identical, the variables are extracted, sorted alphabetically, and stored in the
 * `sorted_vars` vector. If the sets differ, an error is reported, and the differences
 * between the sets are logged.
 *
 * @param expr1_raw The raw string representation of the first expression.
 * @param expr2_raw The raw string representation of the second expression.
 * @param sorted_vars A vector to store the sorted list of unique variable names if the
 *                    variable sets from both expressions match. This vector is cleared if
 *                    the sets do not match.
 *
 * @return `true` if the variable sets from both expressions match, indicating that the
 *         `sorted_vars` vector contains the sorted list of unique variable names.
 *         `false` if the variable sets do not match, indicating an error.
 *
 * @note The `isIdentifier` function is used to validate potential variable names.
 * @note The `keywords` set contains a list of reserved words that cannot be used as variable names.
 * @note The function uses spdlog for logging debug, trace, info, and warning messages.
 */
bool LogicComparator::extractVariables(const std::string &expr1_raw, const std::string &expr2_raw,
                                       std::vector<std::string> &sorted_vars) {
  // Define the set of ExprTk keywords/function names to filter out (lowercase)
  // Even though isIdentifier checks for uppercase, keep this filter as a safety measure
  static const std::set<std::string> keywords = {"abs",
                                                 "acos",
                                                 "acosh",
                                                 "and",
                                                 "asin",
                                                 "asinh",
                                                 "assert",
                                                 "atan",
                                                 "atan2",
                                                 "atanh",
                                                 "avg",
                                                 "break",
                                                 "case",
                                                 "ceil",
                                                 "clamp",
                                                 "continue",
                                                 "cosh",
                                                 "cos",
                                                 "cot",
                                                 "csc",
                                                 "default",
                                                 "deg2grad",
                                                 "deg2rad",
                                                 "else",
                                                 "equal",
                                                 "erfc",
                                                 "erf",
                                                 "exp",
                                                 "expm1",
                                                 "false",
                                                 "floor",
                                                 "for",
                                                 "frac",
                                                 "grad2deg",
                                                 "hypot",
                                                 "iclamp",
                                                 "if",
                                                 "ilike",
                                                 "in",
                                                 "inrange",
                                                 /*"in",*/ "like",
                                                 "log",
                                                 "log10",
                                                 "log1p",
                                                 "log2",
                                                 "logn",
                                                 "mand",
                                                 "max",
                                                 "min",
                                                 "mod",
                                                 "mor",
                                                 "mul",
                                                 "nand",
                                                 "ncdf",
                                                 "nor",
                                                 "not",
                                                 "not_equal",
                                                 /*"not",*/ "null",
                                                 "or",
                                                 "pow",
                                                 "rad2deg",
                                                 "repeat",
                                                 "return",
                                                 "root",
                                                 "roundn",
                                                 "round",
                                                 "sec",
                                                 "sgn",
                                                 "shl",
                                                 "shr",
                                                 "sinc",
                                                 "sinh",
                                                 "sin",
                                                 "sqrt",
                                                 "sum",
                                                 "swap",
                                                 "switch",
                                                 "tanh",
                                                 "tan",
                                                 "true",
                                                 "trunc",
                                                 "until",
                                                 "var",
                                                 "while",
                                                 "xnor",
                                                 "xor"};

  // Regular expression to find potential identifiers (unchanged)
  const std::regex identifier_regex("[a-zA-Z_][a-zA-Z0-9_]*");

  // Store validated and filtered variable names
  std::set<std::string> actual_vars1_set;
  std::set<std::string> actual_vars2_set;

  // --- Helper function: build log string ---
  auto build_log_string = [](const auto &container) {
    std::ostringstream oss;
    oss << "[";
    for (auto it = container.begin(); it != container.end(); ++it) {
      oss << *it;
      if (std::next(it) != container.end()) {
        oss << ", ";
      }
    }
    oss << "]";
    return oss.str();
  };

  // --- Process the first expression ---
  spdlog::debug("Extracting and validating identifiers from raw expr1: {}", expr1_raw);
  auto words_begin1 = std::sregex_iterator(expr1_raw.begin(), expr1_raw.end(), identifier_regex);
  auto words_end1 = std::sregex_iterator();
  for (std::sregex_iterator i = words_begin1; i != words_end1; ++i) {
    std::string potential_var = i->str();
    spdlog::trace("Regex found in expr1: {}", potential_var);

    // 1. Validate using isIdentifier (now checks for uppercase etc.)
    if (isIdentifier(potential_var)) {
      // 2. Check if it's a keyword (still use lowercase comparison as a precaution)
      std::string lower_var = potential_var;
      std::transform(lower_var.begin(), lower_var.end(), lower_var.begin(),
                     [](unsigned char c) { return std::tolower(c); });

      if (keywords.find(lower_var) == keywords.end()) {
        actual_vars1_set.insert(potential_var); // Keep original case
        spdlog::trace("Kept variable from expr1: {}", potential_var);
      } else {
        spdlog::trace("Filtered keyword from expr1: {}", potential_var);
      }
    } else {
      spdlog::trace("Filtered invalid identifier from expr1: {}", potential_var);
    }
  }
  spdlog::debug("Validated variables found in expr1: {}", build_log_string(actual_vars1_set));

  // --- Process the second expression ---
  spdlog::debug("Extracting and validating identifiers from raw expr2: {}", expr2_raw);
  auto words_begin2 = std::sregex_iterator(expr2_raw.begin(), expr2_raw.end(), identifier_regex);
  auto words_end2 = std::sregex_iterator();
  for (std::sregex_iterator i = words_begin2; i != words_end2; ++i) {
    std::string potential_var = i->str();
    spdlog::trace("Regex found in expr2: {}", potential_var);

    // 1. Validate using isIdentifier
    if (isIdentifier(potential_var)) {
      // 2. Check if it's a keyword (still use lowercase comparison)
      std::string lower_var = potential_var;
      std::transform(lower_var.begin(), lower_var.end(), lower_var.begin(),
                     [](unsigned char c) { return std::tolower(c); });

      if (keywords.find(lower_var) == keywords.end()) {
        actual_vars2_set.insert(potential_var); // Keep original case
        spdlog::trace("Kept variable from expr2: {}", potential_var);
      } else {
        spdlog::trace("Filtered keyword from expr2: {}", potential_var);
      }
    } else {
      spdlog::trace("Filtered invalid identifier from expr2: {}", potential_var);
    }
  }
  spdlog::debug("Validated variables found in expr2: {}", build_log_string(actual_vars2_set));

  // --- Compare the two variable sets ---
  if (actual_vars1_set == actual_vars2_set) {
    // Sets are equal, extract the variable list
    sorted_vars.assign(actual_vars1_set.begin(), actual_vars1_set.end());
    std::sort(sorted_vars.begin(), sorted_vars.end()); // Sort alphabetically
    spdlog::info("Variable sets match. Found unique variables for comparison: {}",
                 build_log_string(sorted_vars));
    spdlog::info("Extracted variables done !");
    return true; // Variable sets are consistent
  } else {
    // Sets are not equal, report an error and return false
    spdlog::warn("Variable sets do not match between the two expressions!");
    // Calculate the differences for more detailed logging
    std::vector<std::string> diff1, diff2;
    std::set_difference(actual_vars1_set.begin(), actual_vars1_set.end(), actual_vars2_set.begin(),
                        actual_vars2_set.end(),
                        std::back_inserter(diff1)); // Vars only in expr1
    std::set_difference(actual_vars2_set.begin(), actual_vars2_set.end(), actual_vars1_set.begin(),
                        actual_vars1_set.end(),
                        std::back_inserter(diff2)); // Vars only in expr2

    if (!diff1.empty()) {
      spdlog::warn("Variables only in reference expression: {}", build_log_string(diff1));
    }
    if (!diff2.empty()) {
      spdlog::warn("Variables only in comparison expression: {}", build_log_string(diff2));
    }
    sorted_vars.clear(); // Clear the output variable list
    return false;        // Variable sets are inconsistent
  }
}

void LogicComparator::generateReport(
    const std::string &output_file,
    const std::map<std::string, PinComparisonResult> &comparison_results) {}