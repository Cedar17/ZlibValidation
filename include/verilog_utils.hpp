#ifndef VERILOG_UTILS_H
#define VERILOG_UTILS_H

#include <fstream>
#include <iostream> // For std::ostream
#include <string>

#include "slang/syntax/SyntaxPrinter.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/syntax/SyntaxVisitor.h"

// Creating custom visitor class
class VerilogVisitor : public slang::syntax::SyntaxVisitor<VerilogVisitor> {
public:
  explicit VerilogVisitor(const std::string &targetCell);
  void handle(const slang::syntax::SyntaxNode &node);
  void handle(const slang::syntax::ModuleDeclarationSyntax &module);
  void handle(const slang::syntax::PortDeclarationSyntax &portDecl);
  void handle(const slang::syntax::HierarchyInstantiationSyntax &hierarchyInst);
  void handle(const slang::syntax::PrimitiveInstantiationSyntax &primitiveInst);
  void handle(const slang::syntax::SpecifyBlockSyntax &specifyBlock);

private:
  std::string targetCell_;
  int depth_;
  bool inTargetModule_;
};

// Creating a Rewriter to extract a specific cell
class CellExtractor : public slang::syntax::SyntaxRewriter<CellExtractor> {
public:
  explicit CellExtractor(const std::string &targetCell);
  void handle(const slang::syntax::ModuleDeclarationSyntax &module);
  bool foundTargetCell() const;

private:
  std::string targetCell_;
  bool foundTarget_;
};

// Print specific cell when visiting SyntaxTree
class CellPrinter : public slang::syntax::SyntaxVisitor<CellPrinter> {
public:
  explicit CellPrinter(const std::string &targetCell, std::ostream &out);
  void handle(const slang::syntax::ModuleDeclarationSyntax &module);

private:
  std::string targetCell_;
  std::ostream &out_;
  bool foundTarget_;
};

void getAST(const std::string &verilog_file, const std::string &cell);

#endif // VERILOG_UTILS_H