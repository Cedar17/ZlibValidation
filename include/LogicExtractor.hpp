// include/LogicExtractor.hpp

#ifndef LOGIC_EXTRACTOR_HPP
#define LOGIC_EXTRACTOR_HPP

#include "verilog_utils.hpp"

// Structure to represent a gate instance's information
struct GateInfo {
  slang::parsing::TokenKind kind; // Store the TokenKind for reliable checks
  std::string gateTypeName;       // Store the string name like "and", "xor"
  std::vector<std::string> inputSignals;
  std::string outputSignal;
};

// Visitor class to extract netlist information and derive logic expressions
class LogicExtractor : public slang::syntax::SyntaxVisitor<LogicExtractor> {
public:
  // Constructor takes the target cell name
  explicit LogicExtractor(const std::string &targetCell)
      : targetCell_(targetCell), inTargetModule_(false), parsingComplete_(false) {}

  // --- Visitor Handlers ---
  void handle(const slang::syntax::ModuleDeclarationSyntax &module);
  void handle(
      const slang::syntax::PortDeclarationSyntax &portDecl); // Handles direction/type declaration
  void
  handle(const slang::syntax::NonAnsiPortListSyntax &portList); // Handles port names list in header
  void
  handle(const slang::syntax::NetDeclarationSyntax &netDecl); // To find explicitly declared wires
  void handle(const slang::syntax::PrimitiveInstantiationSyntax
                  &primitiveInst); // Handles gate instantiation
  // Potentially handle ContinuousAssignSyntax if needed later:
  // void handle(const slang::syntax::ContinuousAssignSyntax& assign);

  // --- Access Extracted Info (for Step 1 debugging) ---
  const std::unordered_map<std::string, GateInfo> &getExtractedGates() const {
    return gateOutputDrivers_;
  }
  const std::unordered_set<std::string> &getPrimaryInputs() const { return primaryInputs_; }
  const std::unordered_set<std::string> &getPrimaryOutputs() const { return primaryOutputs_; }
  const std::unordered_set<std::string> &getInternalWires() const { return internalWires_; }

  // --- Logic Derivation (Commented out for Step 1) ---
  std::map<std::string, std::string> getLogicExpressions();

private:
  // --- Internal State ---
  const std::string &targetCell_;
  bool inTargetModule_;
  bool parsingComplete_; // Flag to indicate AST traversal is done

  // Netlist Information
  std::unordered_set<std::string> primaryInputs_;
  std::unordered_set<std::string> primaryOutputs_;
  std::unordered_set<std::string> internalWires_; // Includes gate outputs

  // Temporary map to store directions found in PortDeclarationSyntax
  // Key: port name, Value: direction ("input", "output", "inout", "unknown")
  std::unordered_map<std::string, std::string> portDirections_;

  // Map: output signal name -> GateInfo driving it
  std::unordered_map<std::string, GateInfo> gateOutputDrivers_;

  // Map: signal name -> Logic expression string (Memoization Cache) - (Used in Step 2)
  std::unordered_map<std::string, std::string> logicCache_;

  // --- Helper Methods ---
  // Recursive function to derive logic for a given signal (Used in Step 2)
  std::string deriveLogicRecursive(const std::string &signalName);
  // Helper to format expressions based on gate type (Used in Step 2)
  std::string formatExpression(const GateInfo &gateInfo,
                               const std::vector<std::string> &inputExprs);
};

// --- Function Declaration ---
// For Step 1, this function will just run the visitor and maybe print summary
void extractAndPrintNetlistInfo(const std::string &verilog_file, const std::string &cell);

// Function to be implemented in Step 2
std::map<std::string, std::string> extractLogicFromVerilog(const std::string &verilog_file,
                                                           const std::string &cell);

#endif // LOGIC_EXTRACTOR_HPP