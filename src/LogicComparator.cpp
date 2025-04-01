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
 * - Start with an alphabetic character.
 * - Contain at least one alphabetic character.
 *
 * @param token The string to check.
 * @return True if the string is a valid identifier, false otherwise.
 */
bool isIdentifier(const std::string &token) {
  if (token.empty())
    return false;
  // Check if the token is a valid identifier (alphanumeric + underscore)
  bool has_alpha = false;
  for (char c : token) {
    if (std::isalpha(c)) {
      has_alpha = true;
    } else if (!std::isalnum(c) && c != '_') {
      return false; // Contains invalid char
    }
  }
  // Must start with an alphabetic character
  return has_alpha && std::isalpha(token[0]);
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
  final_expr = std::regex_replace(final_expr, std::regex(" \\) "), ")");
  final_expr = std::regex_replace(final_expr, std::regex("\\) "), ")");
  final_expr = std::regex_replace(final_expr, std::regex("^\\s+|\\s+$"), "");

  spdlog::debug("Preprocessed expression result: {}", final_expr);
  return final_expr;
}

void LogicComparator::generateReport(
    const std::string &output_file,
    const std::map<std::string, PinComparisonResult> &comparison_results) {}