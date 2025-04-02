#include "LogicComparator.hpp"

LogicComparator::LogicComparator(const std::map<std::string, std::string> &ref_outpin_map,
                                 const std::map<std::string, std::string> &comp_outpin_map,
                                 const std::string &cell_name)
    : ref_outpin_map_(ref_outpin_map), comp_outpin_map_(comp_outpin_map), cell_name_(cell_name) {}
void LogicComparator::logic() {
  typedef exprtk::symbol_table<double> symbol_table_t;
  typedef exprtk::expression<double> expression_t;
  typedef exprtk::parser<double> parser_t;

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
    symbol_table.get_variable("A")->ref() = double(i & 0x01 ? 1 : 0);
    symbol_table.get_variable("B")->ref() = double(i & 0x02 ? 1 : 0);
    symbol_table.get_variable("C")->ref() = double(i & 0x04 ? 1 : 0);

    const int result = static_cast<int>(expression.value());

    printf(" %d | %d | %d | %d | %d \n", i,
           static_cast<int>(symbol_table.get_variable("A")->value()),
           static_cast<int>(symbol_table.get_variable("B")->value()),
           static_cast<int>(symbol_table.get_variable("C")->value()), result);
  }
}

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
  if (processed.empty()) {
    spdlog::debug("Input expression is empty.");
    return ""; // Handle empty input early
  }

  // --- Helper function: build log string ---
  auto build_log_string = [](const auto &container) {
    std::ostringstream oss;
    for (auto it = container.begin(); it != container.end(); ++it) {
      oss << *it;
      if (std::next(it) != container.end()) {
        oss << ", ";
      }
    }
    return oss.str();
  };

  // 1. Handle '!' for NOT carefully BEFORE adding general spaces.
  // Replace logical NOT '!' with " not ", ensuring not to replace '!='.
  std::string temp_processed;
  temp_processed.reserve(processed.length() * 1.2); // Pre-allocate a bit more space
  for (size_t i = 0; i < processed.length(); ++i) {
    if (processed[i] == '!') {
      if (i + 1 < processed.length() && processed[i + 1] == '=') {
        temp_processed += "!="; // Keep inequality operator
        i++;                    // Skip the '='
      } else {
        temp_processed += " not "; // Replace logical NOT with spaced operator
      }
    } else {
      temp_processed += processed[i];
    }
  }
  processed = temp_processed;
  spdlog::trace("After '!' to 'not' conversion: {}", processed);

  // 2. Add spaces around other operators and parentheses that need them
  processed = std::regex_replace(processed, std::regex("\\("), " ( ");
  processed = std::regex_replace(processed, std::regex("\\)"), " ) ");
  processed = std::regex_replace(processed, std::regex("\\+"), " + "); // OR
  processed = std::regex_replace(processed, std::regex("\\^"), " ^ "); // XOR
  processed = std::regex_replace(processed, std::regex("\\*"), " * "); // AND
  // Consolidate multiple spaces into one and trim again
  processed = std::regex_replace(processed, std::regex("\\s+"), " ");
  processed = std::regex_replace(processed, std::regex("^\\s+|\\s+$"), "");
  spdlog::trace("After adding spaces: {}", processed);

  // 3. Replace symbolic operators with their keyword equivalents
  //    Note: 'not' is already handled.
  processed = std::regex_replace(processed, std::regex(" \\+ "), " or ");
  processed = std::regex_replace(processed, std::regex(" \\^ "), " xor ");
  processed = std::regex_replace(processed, std::regex(" \\* "), " and ");
  spdlog::trace("After operator keyword replacement: {}", processed);

  // 4. Tokenize based on spaces
  std::stringstream ss(processed);
  // Read tokens skipping whitespace
  std::vector<std::string> tokens{std::istream_iterator<std::string>(ss),
                                  std::istream_iterator<std::string>()};

  if (tokens.empty()) {
    spdlog::debug("No tokens found after tokenization.");
    return "";
  }
  spdlog::trace("Tokens after initial processing: [{}]", build_log_string(tokens));

  // 5. Insert implied 'and'
  std::vector<std::string> tokens_with_implied_and;
  if (!tokens.empty()) {
    tokens_with_implied_and.push_back(tokens[0]);
    for (size_t i = 0; i < tokens.size() - 1; ++i) {
      const std::string &current_token = tokens[i];
      const std::string &next_token = tokens[i + 1];

      // An operand ends if it's an identifier or a closing parenthesis.
      bool current_ends_operand = isIdentifier(current_token) || current_token == ")";
      // An operand starts if it's an identifier, an opening parenthesis, or 'not'.
      bool next_starts_operand =
          isIdentifier(next_token) || next_token == "(" || next_token == "not";

      // We should insert 'and' if the current token ends an operand AND the next token starts one,
      // UNLESS the current token is already an operator/opening bracket OR the next token is an
      // operator/closing bracket. Explicit check: Don't insert 'and' if an operator already exists
      // between them.
      bool current_is_op_or_open =
          isOperator(current_token) || current_token == "not" || current_token == "(";
      bool next_is_op_or_close = isOperator(next_token) || next_token == "not" || next_token == ")";

      if (current_ends_operand && next_starts_operand && !current_is_op_or_open &&
          !next_is_op_or_close) {
        spdlog::trace("Inserting implied 'and' between '{}' and '{}'", current_token, next_token);
        tokens_with_implied_and.push_back("and");
      }

      tokens_with_implied_and.push_back(next_token); // Add the next token regardless
    }
  } else {
    // Handle case where initial tokenization yielded no tokens
    return "";
  }

  spdlog::trace("Tokens after implied 'and': [{}]", build_log_string(tokens_with_implied_and));

  // 6. Handle 'not Identifier' ONLY during reconstruction (NEW LOGIC)
  std::vector<std::string> final_tokens;
  for (size_t i = 0; i < tokens_with_implied_and.size(); ++i) {
    const std::string &current_token = tokens_with_implied_and[i];

    if (current_token == "not") {
      // Check if the *next* token is an Identifier
      if (i + 1 < tokens_with_implied_and.size() && isIdentifier(tokens_with_implied_and[i + 1])) {
        // Found 'not Identifier', transform to 'not(Identifier)'
        final_tokens.push_back("not");
        final_tokens.push_back("(");
        final_tokens.push_back(tokens_with_implied_and[i + 1]); // The identifier
        final_tokens.push_back(")");
        i++; // Increment i to skip the identifier we just processed
      } else {
        // 'not' is followed by '(', another operator, or is at the end.
        // Add 'not' as is, let ExprTk parse 'not(...)' or handle errors.
        final_tokens.push_back("not");
      }
    } else {
      // Token is not 'not', just add it to the final list
      final_tokens.push_back(current_token);
    }
  }
  spdlog::trace("Tokens after 'not' parenthesis handling: [{}]", build_log_string(final_tokens));

  // 7. Reconstruct the final expression string from tokens
  std::string final_expr;
  if (!final_tokens.empty()) {
    final_expr = final_tokens[0];
    for (size_t i = 1; i < final_tokens.size(); ++i) {
      const std::string &prev = final_tokens[i - 1];
      const std::string &curr = final_tokens[i];

      // Add space smartly: No space after '(', no space before ')',
      // no space after 'not' if followed by '(', no space before ',' (in function args, if
      // applicable)
      bool add_space = true;
      if (prev == "(" || curr == ")" || curr == ",") {
        add_space = false;
      }
      if (prev == "not" && curr == "(") {
        add_space = false; // Already handled by 'not(...)' structure
      }
      // Potentially add more rules if needed for other operators/functions

      if (add_space) {
        final_expr += " ";
      }
      final_expr += curr;
    }
  }

  // 8. Final cleanup (consolidate any remaining multiple spaces and trim)
  final_expr = std::regex_replace(final_expr, std::regex("\\s+"), " ");
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

// --- Implementation of compareSingleExpressionPair ---

/**
 * @brief Compares two preprocessed logic expression strings for equivalence using truth tables.
 *        Internal helper function.
 *
 * @tparam double The numeric type used by ExprTk (e.g., double, float).
 * @param ref_expression_processed The preprocessed reference logic expression.
 * @param comp_expression_processed The preprocessed comparison logic expression.
 * @param sorted_vars A sorted vector of unique variable names common to both expressions.
 * @param result Reference to a PinComparisonResult object to store detailed results.
 */

void LogicComparator::compareSingleExpressionPair(const std::string &ref_expression_processed,
                                                  const std::string &comp_expression_processed,
                                                  const std::vector<std::string> &sorted_vars,
                                                  PinComparisonResult &result) {
  // Store processed expressions in the result struct
  result.ref_expr_processed = ref_expression_processed;
  result.comp_expr_processed = comp_expression_processed;
  result.comparison_possible = true; // Assume comparison is possible initially

  spdlog::debug("Comparing processed expressions for pin '{}':", result.pin_name);
  spdlog::debug("  Ref : {}", ref_expression_processed);
  spdlog::debug("  Comp: {}", comp_expression_processed);

  // --- ExprTk Setup ---
  typedef exprtk::symbol_table<double> symbol_table_t;
  typedef exprtk::expression<double> expression_t;
  typedef exprtk::parser<double> parser_t;

  // Setup for Reference Expression
  symbol_table_t ref_symbol_table;
  expression_t ref_expression;
  parser_t ref_parser;

  for (const auto &var_name : sorted_vars) {
    // Create variable, ExprTk manages its memory within the table
    if (!ref_symbol_table.create_variable(var_name)) {
      result.error_message = "Failed to create variable '" + var_name + "' in ref symbol table.";
      spdlog::error(result.error_message);
      result.comparison_possible = false;
      return; // Cannot proceed
    }
  }
  ref_expression.register_symbol_table(ref_symbol_table);

  // Compile Reference Expression
  result.ref_compiles = ref_parser.compile(ref_expression_processed, ref_expression);
  if (!result.ref_compiles) {
    result.error_message = "Reference expression compilation failed: " + ref_parser.error();
    spdlog::warn("Pin '{}': {}", result.pin_name, result.error_message);
    result.comparison_possible = false;
    // Don't return yet, maybe the comparison one also fails
  } else {
    spdlog::debug("Pin '{}': Reference expression compiled successfully.", result.pin_name);
  }

  // Setup for Comparison Expression
  symbol_table_t comp_symbol_table;
  expression_t comp_expression;
  parser_t comp_parser;

  for (const auto &var_name : sorted_vars) {
    if (!comp_symbol_table.create_variable(var_name)) {
      // Append error if ref also failed, otherwise set it
      std::string comp_err = "Failed to create variable '" + var_name + "' in comp symbol table.";
      result.error_message += (result.error_message.empty() ? "" : "\n") + comp_err;
      spdlog::error(comp_err);
      result.comparison_possible = false; // Mark as impossible now
      return;                             // Cannot proceed if variable creation fails
    }
  }
  comp_expression.register_symbol_table(comp_symbol_table);

  // Compile Comparison Expression
  result.comp_compiles = comp_parser.compile(comp_expression_processed, comp_expression);
  if (!result.comp_compiles) {
    std::string comp_err = "Comparison expression compilation failed: " + comp_parser.error();
    result.error_message += (result.error_message.empty() ? "" : "\n") + comp_err;
    spdlog::warn("Pin '{}': {}", result.pin_name, comp_err);
    result.comparison_possible = false; // Mark as impossible if not already
  } else {
    spdlog::debug("Pin '{}': Comparison expression compiled successfully.", result.pin_name);
  }

  // If either failed to compile, comparison is not possible
  if (!result.ref_compiles || !result.comp_compiles) {
    result.comparison_possible = false;
    return;
  }

  // --- Truth Table Generation and Comparison ---
  size_t num_vars = sorted_vars.size();
  // Use unsigned long long for potentially large number of combinations
  unsigned long long num_combinations = 1ULL << num_vars;
  // Prevent excessively large tables (e.g., > 20 variables is 1M+ rows)
  const unsigned long long MAX_COMBINATIONS = 1ULL << 20; // Limit to 2^20 combinations
  if (num_vars > 20) {                                    // Check against var count for clarity
    result.error_message = "Too many variables (" + std::to_string(num_vars) +
                           "). Max supported for truth table is 20.";
    spdlog::error("Pin '{}': {}", result.pin_name, result.error_message);
    result.comparison_possible = false;
    return;
  }
  if (num_combinations == 0 && num_vars > 0) { // Overflow check
    result.error_message = "Number of combinations calculation overflowed for " +
                           std::to_string(num_vars) + " variables.";
    spdlog::error("Pin '{}': {}", result.pin_name, result.error_message);
    result.comparison_possible = false;
    return;
  }

  std::vector<bool> ref_results;
  std::vector<bool> comp_results;
  ref_results.reserve(static_cast<size_t>(num_combinations)); // Avoid reallocations
  comp_results.reserve(static_cast<size_t>(num_combinations));

  // --- Prepare Tabulate Tables ---
  Table ref_table;
  Table comp_table;
  Table::Row_t header_row;
  header_row.push_back("#"); // Row number column
  for (const auto &var_name : sorted_vars) {
    header_row.push_back(var_name);
  }
  header_row.push_back(ref_expression_processed); // Reference expression as last column header
  ref_table.add_row(header_row);

  // Adjust header for comparison table
  header_row.back() = comp_expression_processed; // Change last header to comparison expression
  comp_table.add_row(header_row);

  // Format header rows
  for (size_t i = 0; i < ref_table[0].size(); ++i) {
    ref_table[0][i].format().font_color(Color::yellow).font_style({FontStyle::bold});
    comp_table[0][i].format().font_color(Color::yellow).font_style({FontStyle::bold});
  }

  spdlog::debug("Pin '{}': Generating truth table with {} variables ({} combinations)...",
                result.pin_name, num_vars, num_combinations);

  bool evaluation_error = false;
  for (unsigned long long i = 0; i < num_combinations; ++i) {
    Table::Row_t ref_data_row;
    Table::Row_t comp_data_row;
    ref_data_row.push_back(std::to_string(i));
    comp_data_row.push_back(std::to_string(i));

    // Set variable values for this combination
    for (size_t j = 0; j < num_vars; ++j) {
      // The j-th bit of i determines the value of the j-th variable
      bool bit_value = ((i >> j) & 1ULL);
      double var_value = bit_value ? double(1.0) : double(0.0); // ExprTk uses floating point

      // Get variable reference and assign value
      // Need error checking here in theory, but create_variable should have caught issues
      ref_symbol_table.get_variable(sorted_vars[j])->ref() = var_value;
      comp_symbol_table.get_variable(sorted_vars[j])->ref() = var_value;

      // Add input value to table rows (as string "0" or "1")
      std::string bit_str = bit_value ? "1" : "0";
      ref_data_row.push_back(bit_str);
      comp_data_row.push_back(bit_str);
    }

    // Evaluate expressions
    double ref_val, comp_val;
    try {
      ref_val = ref_expression.value();
    } catch (const std::exception &e) {
      result.error_message +=
          "\nReference evaluation failed at combination " + std::to_string(i) + ": " + e.what();
      spdlog::error("Pin '{}': {}", result.pin_name, result.error_message);
      result.comparison_possible = false;
      evaluation_error = true;
      break; // Stop evaluation
    }
    try {
      comp_val = comp_expression.value();
    } catch (const std::exception &e) {
      result.error_message +=
          "\nComparison evaluation failed at combination " + std::to_string(i) + ": " + e.what();
      spdlog::error("Pin '{}': {}", result.pin_name, result.error_message);
      result.comparison_possible = false;
      evaluation_error = true;
      break; // Stop evaluation
    }

    // Store boolean results (commonly, non-zero is true in logic contexts)
    bool ref_bool_result = (ref_val != double(0.0));
    bool comp_bool_result = (comp_val != double(0.0));

    ref_results.push_back(ref_bool_result);
    comp_results.push_back(comp_bool_result);

    // Add output results to table rows (as string "0" or "1")
    ref_data_row.push_back(ref_bool_result ? "1" : "0");
    comp_data_row.push_back(comp_bool_result ? "1" : "0");

    // Add rows to tables
    ref_table.add_row(ref_data_row);
    comp_table.add_row(comp_data_row);

  } // End of combination loop

  if (evaluation_error) {
    return; // Don't proceed if evaluation failed
  }

  // --- Final Comparison ---
  if (result.comparison_possible) {
    result.are_equivalent = (ref_results == comp_results);
    if (result.are_equivalent) {
      spdlog::info("Pin '{}': Expressions ARE logically equivalent.", result.pin_name);
    } else {
      spdlog::warn("Pin '{}': Expressions ARE NOT logically equivalent.", result.pin_name);
      result.error_message +=
          "\nTruth table outputs differ."; // Add specific reason for non-equivalence
    }

    // Store the generated tables in the result struct
    result.ref_truth_table = ref_table;
    result.comp_truth_table = comp_table;
  }
}

/**
 * @brief Compares logic for all output pins defined in the input maps for a specific cell.
 *
 * Iterates through all unique output pins found in either the reference or
 * comparison maps. For each pin, it preprocesses the expressions, extracts
 * and validates variables, and then compares the logic using truth tables via
 * compareSingleExpressionPair.
 *
 * @tparam double Numeric type for ExprTk (e.g., double).
 * @return A map where the key is the pin name and the value is the
 *         PinComparisonResult struct containing detailed comparison results for that pin.
 */

void LogicComparator::compareCellLogic() {

  // 1. Collect all unique pin names from both maps
  std::set<std::string> unique_pin_names;
  for (const auto &pair : ref_outpin_map_) {
    unique_pin_names.insert(pair.first);
  }
  for (const auto &pair : comp_outpin_map_) {
    unique_pin_names.insert(pair.first);
  }

  spdlog::info("Starting logic comparison for cell '{}' with {} unique output pins...", cell_name_,
               unique_pin_names.size());

  // 2. Iterate through each unique pin name
  for (const std::string &pin_name : unique_pin_names) {
    PinComparisonResult pin_result;
    pin_result.pin_name = pin_name;
    pin_result.comparison_possible = true; // Assume possible initially

    // 3. Get raw expressions
    auto ref_it = ref_outpin_map_.find(pin_name);
    auto comp_it = comp_outpin_map_.find(pin_name);

    if (ref_it == ref_outpin_map_.end()) {
      pin_result.error_message = "Pin not found in reference map.";
      spdlog::warn("Cell '{}', Pin '{}': {}", cell_name_, pin_name, pin_result.error_message);
      pin_result.comparison_possible = false;
      // Still try to get the comparison expression for reporting
      if (comp_it != comp_outpin_map_.end()) {
        pin_result.comp_expr_raw = comp_it->second;
      }
    } else {
      pin_result.ref_expr_raw = ref_it->second;
      spdlog::debug("Pin -> Expression: {} -> {}", pin_name, pin_result.ref_expr_raw);
    }

    if (comp_it == comp_outpin_map_.end()) {
      std::string comp_err = "Pin not found in comparison map.";
      pin_result.error_message += (pin_result.error_message.empty() ? "" : "\n") + comp_err;
      spdlog::warn("Cell '{}', Pin '{}': {}", cell_name_, pin_name, comp_err);
      pin_result.comparison_possible = false;
      // If ref existed, store it
      if (ref_it != ref_outpin_map_.end()) {
        pin_result.ref_expr_raw = ref_it->second; // Already stored if ref exists
      }
    } else {
      pin_result.comp_expr_raw = comp_it->second;
      spdlog::debug("Pin => Expression: {} => {}", pin_name, pin_result.comp_expr_raw);
    }

    // If pin missing in either, we can't compare, but store result and continue
    if (!pin_result.comparison_possible) {
      all_pin_results_[pin_name] = pin_result;
      continue;
    }

    // 4. Preprocess expressions
    std::string ref_processed = preprocessExpression(pin_result.ref_expr_raw);
    std::string comp_processed = preprocessExpression(pin_result.comp_expr_raw);

    if (ref_processed.empty()) {
      pin_result.error_message += "\nReference expression became empty after preprocessing.";
      spdlog::warn("Cell '{}', Pin '{}': {}", cell_name_, pin_name,
                   "Reference expression became empty after preprocessing.");
      pin_result.comparison_possible = false;
    }
    if (comp_processed.empty()) {
      std::string comp_err = "\nComparison expression became empty after preprocessing.";
      pin_result.error_message += (pin_result.error_message.empty() ? "" : "\n") + comp_err;
      spdlog::warn("Cell '{}', Pin '{}': {}", cell_name_, pin_name,
                   "Comparison expression became empty after preprocessing.");
      pin_result.comparison_possible = false; // Mark as impossible if not already
    }
    // If preprocessing failed for either, store and continue
    if (ref_processed.empty() || comp_processed.empty()) {
      pin_result.ref_expr_processed = ref_processed; // Store possibly empty strings
      pin_result.comp_expr_processed = comp_processed;
      all_pin_results_[pin_name] = pin_result;
      continue;
    }

    // 5. Extract and validate variables
    std::vector<std::string> sorted_vars;
    // Pass RAW expressions to extractVariables as it uses regex on original format
    bool variables_match =
        extractVariables(pin_result.ref_expr_raw, pin_result.comp_expr_raw, sorted_vars);

    if (!variables_match) {
      pin_result.error_message = "Variable sets do not match between expressions.";
      // extractVariables already logs details
      pin_result.comparison_possible = false;
      pin_result.ref_expr_processed = ref_processed; // Store processed even if vars mismatch
      pin_result.comp_expr_processed = comp_processed;
      all_pin_results_[pin_name] = pin_result;
      continue;
    }
    // --- Helper function: build log string ---
    auto build_log_string = [](const auto &container) {
      std::ostringstream oss;
      for (auto it = container.begin(); it != container.end(); ++it) {
        oss << *it;
        if (std::next(it) != container.end()) {
          oss << ", ";
        }
      }
      return oss.str();
    };
    spdlog::info("Pin '{}': Variable sets match. Found unique variables: {}", pin_name,
                 build_log_string(sorted_vars));

    // 6. Compare the single pair using the processed expressions
    // Pass pin_result by reference - it will be populated by the function
    compareSingleExpressionPair(ref_processed, comp_processed, sorted_vars, pin_result);

    // 7. Store the detailed result for this pin
    all_pin_results_[pin_name] = pin_result;

  } // End of pin loop

  spdlog::info("Logic comparison finished for cell '{}'.", cell_name_);
}

void LogicComparator::generateReport(const std::string &output_file) {

  std::ofstream outfile(output_file);
  if (!outfile.is_open()) {
    spdlog::error("Failed to open output report file: {}", output_file);
    return;
  }

  outfile << "# Logic Equivalence Comparison Report\n\n";
  outfile << "**Cell Name: " << cell_name_ << "**\n\n";
  // Assuming ref_lib_path_ and comp_lib_path_ are accessible if needed
  // outfile << "**Reference Source: [Path to Ref]**\n";
  // outfile << "**Comparison Source: [Path to Comp]**\n\n";

  auto now = std::chrono::system_clock::now();
  std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
  // Use localtime_s or equivalent thread-safe version if available/necessary
  // std::tm now_tm;
  // localtime_s(&now_tm, &now_time_t); // Example for MSVC/C11
  std::tm *now_tm_ptr = std::localtime(&now_time_t); // Standard C++, potentially not thread-safe
  if (now_tm_ptr) {
    outfile << "**Generated on: " << std::put_time(now_tm_ptr, "%Y-%m-%d %H:%M:%S") << "**\n\n";
  }

  outfile << "## Legend\n\n";
  outfile << "| Symbol | Meaning                       |\n";
  outfile << "| :----: | :---------------------------- |\n";
  outfile << "|   ✅   | Logically Equivalent          |\n";
  outfile << "|   ❌   | Not Logically Equivalent      |\n";
  outfile << "|   ➖   | Comparison Not Possible       |\n";
  outfile << "|   ⚠️V  | Variable Mismatch           |\n";
  outfile << "|   ⚠️C  | Compilation Error           |\n";
  outfile << "|   ⚠️E  | Evaluation Error            |\n";
  outfile << "|   ⚠️P  | Preprocessing/Pin Error     |\n";
  outfile << "\n";

  spdlog::info("Generating report for cell '{}' to '{}'...", cell_name_, output_file);

  tabulate::MarkdownExporter exporter; // Create exporter once

  for (const auto &[pin_name, result] : all_pin_results_) {
    outfile << "## Pin: " << pin_name << "\n\n";

    Table summary_table;
    summary_table.add_row({"Property", "Value"});
    summary_table[0][0].format().font_style({FontStyle::bold});
    summary_table[0][1].format().font_style({FontStyle::bold});

    // Determine Status Symbol
    std::string status_symbol;
    if (!result.comparison_possible) {
      status_symbol = "➖";
      // Add specific warning symbol based on error message content maybe?
      if (result.error_message.find("Variable sets do not match") != std::string::npos)
        status_symbol += " (⚠️V)";
      else if (result.error_message.find("compilation failed") != std::string::npos)
        status_symbol += " (⚠️C)";
      else if (result.error_message.find("evaluation failed") != std::string::npos)
        status_symbol += " (⚠️E)";
      else
        status_symbol += " (⚠️P)"; // General preprocessing/pin error
    } else if (result.are_equivalent) {
      status_symbol = "✅";
    } else {
      status_symbol = "❌";
    }

    summary_table.add_row({"Status", status_symbol});
    summary_table.add_row({"Reference (Raw)", "`" + result.ref_expr_raw + "`"});
    summary_table.add_row({"Comparison (Raw)", "`" + result.comp_expr_raw + "`"});
    summary_table.add_row({"Reference (Processed)", "`" + result.ref_expr_processed + "`"});
    summary_table.add_row({"Comparison (Processed)", "`" + result.comp_expr_processed + "`"});
    summary_table.add_row({"Ref Compiles", result.ref_compiles ? "Yes" : "No"});
    summary_table.add_row({"Comp Compiles", result.comp_compiles ? "Yes" : "No"});

    if (!result.error_message.empty()) {
      // Format error message for Markdown (e.g., replace newlines with <br>)
      std::string formatted_error = result.error_message;
      // Basic newline replacement for Markdown
      size_t pos = 0;
      while ((pos = formatted_error.find('\n', pos)) != std::string::npos) {
        formatted_error.replace(pos, 1, "<br>");
        pos += 4; // Length of "<br>"
      }
      summary_table.add_row({"Details/Error", formatted_error});
    }

    // Output summary table
    outfile << exporter.dump(summary_table) << "\n";

    // Optional: Output truth tables if they are different but comparable
    if (!result.are_equivalent && result.comparison_possible &&
        result.ref_truth_table.has_value() && result.comp_truth_table.has_value()) {
      outfile << "### Truth Tables (Differences Detected)\n\n";

      outfile << "#### Reference Truth Table\n\n";
      Table ref_table = result.ref_truth_table.value();
      outfile << exporter.dump(ref_table) << "\n";

      outfile << "#### Comparison Truth Table\n\n";
      Table comp_table = result.comp_truth_table.value();
      outfile << exporter.dump(comp_table) << "\n";
    }
    outfile << "---\n\n"; // Separator between pins

  } // End of pin results loop

  outfile.close();
  spdlog::info("Report generation complete for cell '{}'.", cell_name_);
}
