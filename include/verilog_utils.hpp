// verilog_utils.hpp

#ifndef VERILOG_UTILS_H
#define VERILOG_UTILS_H

#include <algorithm> // For std::find
#include <fstream>
#include <iostream>
#include <map>
#include <memory>    // For shared_ptr
#include <stdexcept> // For exceptions
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "slang/parsing/Token.h" // Include Token header for TokenKind
#include "slang/syntax/AllSyntax.h" // Include for specific syntax nodes like NonAnsiPortListSyntax etc.
#include "slang/syntax/SyntaxPrinter.h"
#include "slang/syntax/SyntaxTree.h" // Include SyntaxTree header
#include "slang/syntax/SyntaxVisitor.h"
#include "spdlog/spdlog.h" // Include spdlog

// Creating custom visitor class
class VerilogVisitor : public slang::syntax::SyntaxVisitor<VerilogVisitor> {
public:
  explicit VerilogVisitor(const std::string &targetCell)
      : targetCell_(targetCell), depth_(0), inTargetModule_(false) {}
  void handle(const slang::syntax::SyntaxNode &node);
  void handle(const slang::syntax::ModuleDeclarationSyntax &module);
  void handle(const slang::syntax::PortDeclarationSyntax &portDecl);
  void handle(const slang::syntax::HierarchyInstantiationSyntax &hierarchyInst);
  // void handle(const slang::syntax::PrimitiveInstantiationSyntax &primitiveInst);
  void handle(const slang::syntax::SpecifyBlockSyntax &specifyBlock);

private:
  const std::string &targetCell_;
  int depth_;
  bool inTargetModule_;
};

// Creating a Rewriter to extract a specific cell
class CellExtractor : public slang::syntax::SyntaxRewriter<CellExtractor> {
public:
  explicit CellExtractor(const std::string &targetCell)
      : targetCell_(targetCell), foundTarget_(false) {}
  void handle(const slang::syntax::ModuleDeclarationSyntax &module);
  bool foundTargetCell() const;

private:
  const std::string &targetCell_;
  bool foundTarget_;
};

// Print specific cell when visiting SyntaxTree
class CellPrinter : public slang::syntax::SyntaxVisitor<CellPrinter> {
public:
  explicit CellPrinter(const std::string &targetCell, std::ostream &out)
      : targetCell_(targetCell), out_(out), foundTarget_(false) {}
  void handle(const slang::syntax::ModuleDeclarationSyntax &module);

private:
  const std::string &targetCell_;
  std::ostream &out_;
  bool foundTarget_;
};

// Comprehensive module rewriter for adding ports, instances, and connections
class ModuleRewriter : public slang::syntax::SyntaxRewriter<ModuleRewriter> {
public:
  explicit ModuleRewriter(const std::vector<std::string> &inputPins,
                          const std::vector<std::string> &outputPins,
                          const std::pair<std::string, std::string> &supercell_entry,
                          int instance_count, std::shared_ptr<spdlog::logger> logger)
      : inputPins_(inputPins), outputPins_(outputPins), cellName_(supercell_entry.first),
        moduleName_(supercell_entry.second), logger_(logger), depth_(0),
        instance_count_(instance_count) {}
  std::shared_ptr<spdlog::logger> logger_;
  void handle(const slang::syntax::SyntaxNode &node);
  void handle(const slang::syntax::ModuleDeclarationSyntax &module);

private:
  const std::vector<std::string> &inputPins_;
  const std::vector<std::string> &outputPins_;
  const std::string cellName_;
  const std::string moduleName_;
  std::map<std::string, std::string> portInfoMap_; // Map from port name to direction
  int depth_;
  int instance_count_;
};

void getAST(const std::string &verilog_file, const std::string &cell);

// --- New Class for Logic Extraction ---

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
  const std::unordered_set<std::string> &getInternalWires() const {
    return internalWires_;
  }

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
// Modify getAST or create a new function to use LogicExtractor
// For Step 1, this function will just run the visitor and maybe print summary
void extractAndPrintNetlistInfo(const std::string &verilog_file, const std::string &cell);

// Function to be implemented in Step 2
std::map<std::string, std::string> extractLogicFromVerilog(const std::string &verilog_file,
                                                           const std::string &cell);

#endif // VERILOG_UTILS_H