#ifndef VERILOG_UTILS_H
#define VERILOG_UTILS_H

#include <fstream>
#include <iostream> // For std::ostream
#include <string>
#include <unordered_set>

#include "slang/syntax/SyntaxPrinter.h"
// #include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxVisitor.h"

// Creating custom visitor class
class VerilogVisitor : public slang::syntax::SyntaxVisitor<VerilogVisitor> {
public:
  explicit VerilogVisitor(const std::string &targetCell)
      : targetCell_(targetCell), depth_(0), inTargetModule_(false) {}
  void handle(const slang::syntax::SyntaxNode &node);
  void handle(const slang::syntax::ModuleDeclarationSyntax &module);
  void handle(const slang::syntax::PortDeclarationSyntax &portDecl);
  void handle(const slang::syntax::HierarchyInstantiationSyntax &hierarchyInst);
  void handle(const slang::syntax::PrimitiveInstantiationSyntax &primitiveInst);
  void handle(const slang::syntax::SpecifyBlockSyntax &specifyBlock);

private:
  const std::string &targetCell_;
  int depth_;
  bool inTargetModule_;
};

// Creating a Rewriter to extract a specific cell
class CellExtractor : public slang::syntax::SyntaxRewriter<CellExtractor> {
public:
  explicit CellExtractor(const std::string &targetCell) : targetCell_(targetCell), foundTarget_(false) {}
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
                          int instance_count,
                          std::shared_ptr<spdlog::logger> logger)
      : inputPins_(inputPins), outputPins_(outputPins), cellName_(supercell_entry.first),
        moduleName_(supercell_entry.second), logger_(logger), depth_(0), instance_count_(instance_count) {}
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

#endif // VERILOG_UTILS_H