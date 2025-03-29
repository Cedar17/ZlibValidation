#include <unordered_set>

#include "spdlog/spdlog.h"

#include "verilog_utils.hpp"

// Adding a generic handler method to record all visited nodes and increment the depth counter
void VerilogVisitor::handle(const slang::syntax::SyntaxNode &node) {
  std::string indent(depth_ * 2, ' ');
  spdlog::debug("{}Node: {}", indent, slang::syntax::toString(node.kind));

  // Increase depth, visit child nodes, then decrease depth
  depth_++;
  visitDefault(node);
  depth_--;
}

// Handle module declarations
void VerilogVisitor::handle(const slang::syntax::ModuleDeclarationSyntax &module) {
  std::string indent(depth_ * 2, ' ');

  if (module.header && module.header->name.valid()) {
    std::string_view moduleName = module.header->name.valueText();
    spdlog::info("{}Module Name:{}", indent, moduleName);

    inTargetModule_ = !targetCell_.empty() && moduleName == targetCell_;
    if (!inTargetModule_) {
      return;
    }

    // If a specific module name is specified, check if it matches
    if (inTargetModule_) {
      spdlog::info("{}Found target module: {}", indent, targetCell_);
      // Print the module ports
      if (module.header->ports) {
        spdlog::info("{}Ports: {}", indent, module.header->ports->toString());
      }
    }
  }

  // Continue processing child nodes
  depth_++;
  visitDefault(module);
  depth_--;
}

// Handle port declarations
void VerilogVisitor::handle(const slang::syntax::PortDeclarationSyntax &portDecl) {
  if (!inTargetModule_) {
    return;
  }

  std::string indent = std::string(depth_ * 2, ' ');

  // Get port direction
  std::string direction = "unknown";
  if (portDecl.header) {
    // Try to convert the header to different types of port headers
    if (auto *varPort = portDecl.header->as_if<slang::syntax::VariablePortHeaderSyntax>()) {
      if (varPort->direction) {
        direction = varPort->direction.valueText();
      }
    } else if (auto *netPort = portDecl.header->as_if<slang::syntax::NetPortHeaderSyntax>()) {
      if (netPort->direction) {
        direction = netPort->direction.valueText();
      }
    }

    // Get port name
    for (const auto &declarator : portDecl.declarators) {
      if (declarator->name.valid()) {
        std::string_view portName = declarator->name.valueText();
        spdlog::info("{}Port Name: {} ({})", indent, portName, direction);
      }
    }
  }
}

// Handle hierarchical instantiation (module instance)
void VerilogVisitor::handle(const slang::syntax::HierarchyInstantiationSyntax &hierarchyInst) {
  if (!inTargetModule_) {
    return;
  }

  std::string indent(depth_ * 2, ' ');

  if (hierarchyInst.type.valid()) {
    std::string_view instType = hierarchyInst.type.valueText();
    spdlog::info("{}Hierarchy Instance Type: {}", indent, instType);

    // Safely print parameter values (if any)
    try {
      if (hierarchyInst.parameters) {
        spdlog::info("{}Parameters:", indent);
        for (const auto &param : hierarchyInst.parameters->parameters) {
          if (!param)
            continue; // Null pointer check

          if (auto *orderedParam = param->as_if<slang::syntax::OrderedParamAssignmentSyntax>()) {
            if (orderedParam->expr) {
              spdlog::info("{}  Parameter: {}", indent, orderedParam->expr->toString());
            }
          } else if (auto *namedParam = param->as_if<slang::syntax::NamedParamAssignmentSyntax>()) {
            if (namedParam->name.valid() && namedParam->expr) {
              spdlog::info("{}  Parameter {}: {}", indent, namedParam->name.valueText(),
                           namedParam->expr->toString());
            }
          }
        }
      }
    } catch (const std::exception &e) {
      spdlog::warn("{}Exception while processing parameters: {}", indent, e.what());
    }

    // Safely handle instances
    try {
      for (const auto &instance : hierarchyInst.instances) {
        if (!instance) {
          spdlog::warn("{}Invalid instance", indent);
          continue;
        } else if (!instance->decl) {
          spdlog::warn("{}Invalid instance declaration", indent);
          // continue;
        } else {
          try {
            if (instance->decl->name.valid()) {
              std::string_view instName = instance->decl->name.valueText();
              spdlog::info("{}Instance Name: {}", indent, instName);
            }
          } catch (...) {
            spdlog::debug("{}Error printing name", indent);
          }

          // Safely print dimensions
          try {
            if (!instance->decl->dimensions.empty()) {
              spdlog::info("{}  Dimensions: {}", indent, instance->decl->dimensions.toString());
            }
          } catch (...) {
            spdlog::debug("{}Error printing dimensions", indent);
          }
        }

        // Safely print port connections
        spdlog::info("{}  Port connections:", indent);
        for (const auto &conn : instance->connections) {
          if (!conn) {
            spdlog::warn("{}    Null connection", indent);
            continue; // Null pointer check
          }

          try {
            // Use as_if method for safe type conversion
            if (auto *ordered = conn->as_if<slang::syntax::OrderedPortConnectionSyntax>()) {
              if (ordered->expr) {
                spdlog::info("{}    Ordered connection: {}", indent, ordered->expr->toString());
              } else {
                spdlog::info("{}    Ordered connection: <empty>", indent);
              }
            } else if (auto *named = conn->as_if<slang::syntax::NamedPortConnectionSyntax>()) {
              if (named->name.valid()) {
                std::string exprStr = named->expr ? named->expr->toString() : "<empty>";
                spdlog::info("{}    .{}({})", indent, named->name.valueText(), exprStr);
              }
            } else if (conn->as_if<slang::syntax::EmptyPortConnectionSyntax>()) {
              spdlog::info("{}    <empty connection>", indent);
            } else if (conn->as_if<slang::syntax::WildcardPortConnectionSyntax>()) {
              spdlog::info("{}    .* (wildcard connection)", indent);
            } else {
              spdlog::info("{}    Unknown connection type: {}", indent,
                           slang::syntax::toString(conn->kind));
            }
          } catch (const std::exception &e) {
            spdlog::warn("{}Exception processing connection: {}", indent, e.what());
          } catch (...) {
            spdlog::warn("{}Unknown exception processing connection", indent);
          }
        }
      }
    } catch (const std::exception &e) {
      spdlog::warn("{}Exception while processing instances: {}", indent, e.what());
    }

    // Continue processing child nodes
    depth_++;
    visitDefault(hierarchyInst);
    depth_--;
  }
}

// Handle primitive gate instantiation
void VerilogVisitor::handle(const slang::syntax::PrimitiveInstantiationSyntax &primitiveInst) {
  if (!inTargetModule_) {
    return;
  }

  std::string indent(depth_ * 2, ' ');

  // Get primitive gate type
  if (primitiveInst.type.valid()) {
    std::string_view gateType = primitiveInst.type.valueText();
    spdlog::info("{}Primitive Gate: {}", indent, gateType);

    // Print delay information (if any)
    if (primitiveInst.delay) {
      spdlog::info("{}Delay: {}", indent, primitiveInst.delay->toString());
    }

    // Print strength information (if any)
    if (primitiveInst.strength) {
      spdlog::info("{}Strength: {}", indent, primitiveInst.strength->toString());
    }

    // Print each instance
    for (const auto &instance : primitiveInst.instances) {
      // Primitive gates usually don't have names, but if they do, print them
      if (instance->decl && instance->decl->name.valid()) {
        std::string_view instName = instance->decl->name.valueText();
        spdlog::info("{}  Gate instance: {}", indent, instName);
      }

      // Print connections
      spdlog::info("{}  Gate connections:", indent);
      for (size_t i = 0; i < instance->connections.size(); ++i) {
        const auto &conn = instance->connections[i];
        // For gate-level instantiation, connections are usually ordered
        if (auto *ordered = conn->as_if<slang::syntax::OrderedPortConnectionSyntax>()) {
          if (ordered->expr) {
            // The first is usually the output, the rest are inputs
            std::string portType = (i == 0) ? "output" : "input";
            spdlog::info("{}    {} {}: {}", indent, portType, i, ordered->expr->toString());
          }
        }
      }
    }

    // Continue to traverse deeper when specific standard gate type
    // Create a set of safe standard gate types
    static const std::unordered_set<std::string_view> safeGateTypes = {
        "and",    "or",     "nand",   "nor",    "xor",      "xnor",    "not",      "buf",     "bufif0",
        "bufif1", "notif0", "notif1", "pullup", "pulldown", "cmos",    "rcmos",    "nmos",    "pmos",
        "rnmos",  "rpmos",  "tran",   "rtran",  "tranif0",  "tranif1", "rtranif0", "rtranif1"};

    if (safeGateTypes.find(gateType) != safeGateTypes.end()) {
      // Continue processing child nodes
      depth_++;
      visitDefault(primitiveInst);
      depth_--;
    } else {
      spdlog::warn("{}Skipping deeper traversal of primitive: {}", indent, gateType);
    }

  } else {
    spdlog::warn("{}Primitive instantiation missing gate type", indent);
  }
}

// Handle specify block
void VerilogVisitor::handle(const slang::syntax::SpecifyBlockSyntax &specifyBlock) {
  if (!inTargetModule_) {
    return;
  }

  std::string indent(depth_ * 2, ' ');
  spdlog::info("{}Specify Block:", indent);

  // Iterate through all path declarations
  for (const auto &item : specifyBlock.items) {
    if (auto *pathDecl = item->as_if<slang::syntax::PathDeclarationSyntax>()) {
      if (pathDecl->desc) {
        std::string pathSrc;
        std::string pathDst;

        // Get path source
        if (!pathDecl->desc->inputs.empty()) {
          // Get the first input
          if (auto *identifier =
                  pathDecl->desc->inputs[0]->as_if<slang::syntax::IdentifierNameSyntax>()) {
            if (identifier->identifier.valid()) {
              pathSrc = identifier->identifier.valueText();
            }
          }
        }

        // Get path destination
        if (pathDecl->desc->suffix) {
          if (auto *simpleSuffix =
                  pathDecl->desc->suffix->as_if<slang::syntax::SimplePathSuffixSyntax>()) {
            if (!simpleSuffix->outputs.empty()) {
              if (auto *identifier =
                      simpleSuffix->outputs[0]->as_if<slang::syntax::IdentifierNameSyntax>()) {
                if (identifier->identifier.valid()) {
                  pathDst = identifier->identifier.valueText();
                }
              }
            } else if (auto *edgeSuffix = pathDecl->desc->suffix
                                              ->as_if<slang::syntax::EdgeSensitivePathSuffixSyntax>()) {
              if (!edgeSuffix->outputs.empty()) {
                if (auto *identifier =
                        edgeSuffix->outputs[0]->as_if<slang::syntax::IdentifierNameSyntax>()) {
                  if (identifier->identifier.valid()) {
                    pathDst = identifier->identifier.valueText();
                  }
                }
              }
            }
          }

          // Construct path string
          std::string pathStr = pathSrc + " => " + pathDst;

          // Get path delay
          std::string delayStr = "(";
          for (size_t i = 0; i < pathDecl->delays.size(); ++i) {
            if (i > 0)
              delayStr += ", ";
            delayStr += pathDecl->delays[i]->toString();
          }
          delayStr += ")";

          spdlog::info("{}  Path: {} = {}", indent, pathStr, delayStr);
        }
      }
      // Can add handling for ConditionalPathDeclarationSyntax and IfNonePathDeclarationSyntax
      else if (auto *condPath = item->as_if<slang::syntax::ConditionalPathDeclarationSyntax>()) {
        if (condPath->path && condPath->predicate) {
          spdlog::info("{}  Conditional Path (if {})", indent, condPath->predicate->toString());
          // Recursively handle internal path
          handle(*condPath->path);
        }
      } else if (auto *ifNonePath = item->as_if<slang::syntax::IfNonePathDeclarationSyntax>()) {
        if (ifNonePath->path) {
          spdlog::info("{}  If-None Path", indent);
          // Recursively handle internal path
          handle(*ifNonePath->path);
        }
      }
    }

    // Continue processing child nodes
    depth_++;
    visitDefault(specifyBlock);
    depth_--;
  }
}

// Handle module declarations, only keep the target module
void CellExtractor::handle(const slang::syntax::ModuleDeclarationSyntax &module) {
  if (module.header && module.header->name.valid()) {
    std::string_view moduleName = module.header->name.valueText();

    // If it is the target module, mark as found, otherwise remove it
    if (!targetCell_.empty() && moduleName == targetCell_) {
      foundTarget_ = true;
      // Do not modify, keep this module
    } else {
      // Remove non-target modules
      remove(module);
    }
  }
}

// Get result
bool CellExtractor::foundTargetCell() const { return foundTarget_; }

// Handle module declarations
void CellPrinter::handle(const slang::syntax::ModuleDeclarationSyntax &module) {
  if (module.header && module.header->name.valid()) {
    std::string_view moduleName = module.header->name.valueText();

    // Check if it is the target module
    if (!targetCell_.empty() && moduleName == targetCell_) {
      foundTarget_ = true;

      // Print module definition
      out_ << "`timescale 1ns/10ps\n";
      out_ << module.toString();

      return; // Do not traverse further
    }
  }

  // Continue traversing other modules
  visitDefault(module);
}

/**
 * @brief Processes a syntax node in the module's AST.
 *
 * This method is called for each syntax node during the traversal of the AST.
 * It logs debug information about the current node and continues processing
 * its child nodes by recursively calling the appropriate visit methods.
 *
 * @param node The syntax node to process
 */
void ModuleRewriter::handle(const slang::syntax::SyntaxNode &node) {
  std::string indent(depth_ * 2, ' ');
  logger_->debug("{}Node: {}", indent, slang::syntax::toString(node.kind));

  // Continue processing child nodes
  depth_++;
  visitDefault(node);
  depth_--;
}

void ModuleRewriter::handle(const slang::syntax::ModuleDeclarationSyntax &module) {
  logger_->debug("Processing module: {}", module.header->name.valueText());

  // Create intermediate wires for connections between instances
  for (int i = 0; i < instance_count_ - 1; i++) {
    auto &newNetNode = parse("\n  wire OP_" + std::to_string(i) + ";");
    insertAtBack(module.members, newNetNode);
    logger_->debug("Added intermediate wire: {}", newNetNode.toString());
  }

  // Get critical input port & output port from
  // the module name pattern: CELLNAME__X#__CRITICALPORT__OUTPUTPORT
  std::string criticalInputPort = "";
  std::string criticalOutputPort = "";
  if (!this->moduleName_.empty()) {
    size_t firstSep = this->moduleName_.find("__");
    if (firstSep != std::string::npos) {
      size_t secondSep = this->moduleName_.find("__", firstSep + 2);
      if (secondSep != std::string::npos) {
        size_t thirdSep = this->moduleName_.find("__", secondSep + 2);
        if (thirdSep != std::string::npos) {
          criticalInputPort = this->moduleName_.substr(secondSep + 2, thirdSep - (secondSep + 2));
          criticalOutputPort = this->moduleName_.substr(thirdSep + 2);
          logger_->debug("Extracted critical input port: {}", criticalInputPort);
          logger_->debug("Extracted output port: {}", criticalOutputPort);
        } else {
          logger_->warn("Can't find thirdSep. Invalid module name pattern: {}", this->moduleName_);
          return;
        }
      } else {
        logger_->warn("Can't find secondSep. Invalid module name pattern: {}", this->moduleName_);
        return;
      }
    } else {
      logger_->warn("Can't find firstSep. Invalid module name pattern: {}", this->moduleName_);
      return;
    }
  } else {
    logger_->warn("Module name is empty. Can't extract critical input port.");
    return;
  }

  // Add additional wires for intermediate outputs that aren't part of the chain
  if (this->outputPins_.size() > 1) {               // Only add if there are multiple output pins
    for (int i = 0; i < instance_count_ - 1; i++) { // Skip the first and last instances
      for (const auto &outputPin : this->outputPins_) {
        if (outputPin != criticalOutputPort) { // Found an intermediate output pin
          auto &newNetNode = parse("\n  wire P_" + std::to_string(i) + "__" + outputPin + ";");
          insertAtBack(module.members, newNetNode);
          logger_->debug("Added intermediate wire: {}", newNetNode.toString());
        }
      }
    }
  }

  std::string portList_str = module.header->ports->toString();
  logger_->debug("Port list: {}", portList_str);

  std::vector<std::string> allPorts;
  auto ansiPortList = module.header->ports->as_if<slang::syntax::AnsiPortListSyntax>();
  if (!ansiPortList) {
    logger_->warn("Port list is not ANSI style.");
    return;
  }
  portInfoMap_.clear(); // Clear the port info map
  for (const auto portMember : ansiPortList->ports) {
    if (portMember->kind == slang::syntax::SyntaxKind::ImplicitAnsiPort) {
      const auto implicitPort = portMember->as_if<slang::syntax::ImplicitAnsiPortSyntax>();
      const auto &directionToken =
          implicitPort->header->as_if<slang::syntax::VariablePortHeaderSyntax>()->direction;
      const auto &nameToken = implicitPort->declarator->name;

      std::string_view portName = nameToken.valueText();
      std::string_view direction = directionToken.valueText();
      portInfoMap_[std::string(portName)] = std::string(direction); // Store port info in the map
      logger_->debug("Port Name: {}, Direction: {}", portName, direction);
    } else {
      logger_->warn("Port member is not ImplicitAnsiPort.");
    }
  }

  for (int i = 0; i < instance_count_; i++) {
    std::string instanceName = "I_" + this->cellName_ + "__X" + std::to_string(i) + "__" +
                               criticalInputPort + "__" + criticalOutputPort;
    std::string instanceCode = "\n  " + this->cellName_ + " " + instanceName + " (";
    std::vector<std::string> portConnections;

    for (const auto &pair : portInfoMap_) {
      std::string portName = pair.first;
      std::string direction = pair.second;
      std::string connectionName;

      if (portName == criticalInputPort) {
        if (i == 0) {
          connectionName = portName; // First instance: connect to module input port
        } else {
          connectionName =
              "OP_" + std::to_string(i - 1); // Intermediate instances: connect to previous OP wire
        }
      } else if (portName == criticalOutputPort) {
        if (i == instance_count_ - 1) {
          connectionName = portName; // Last instance: connect to module output port
        } else {
          connectionName = "OP_" + std::to_string(i); // Intermediate instances: connect to OP wire
        }
      } else if (direction == "output") { // Handle other output ports
        if (i < instance_count_ - 1) {
          connectionName =
              "P_" + std::to_string(i) + "__" + portName; // Connect to intermediate P_i__portName wire
        } else {
          connectionName = portName; // Last instance: connect to module output port
        }
      } else { // For input ports (and inout?), connect directly to module port
        connectionName = portName;
      }
      portConnections.push_back("." + portName + "(" + connectionName + ")");
    }

    // Connect all ports with comma and space
    if (!portConnections.empty()) {
      instanceCode += portConnections[0];
      for (size_t i = 1; i < portConnections.size(); ++i) {
        instanceCode += ", " + portConnections[i];
      }
    }
    instanceCode += ");";

    // Blank line before the first instance
    if (i == 0) {
      instanceCode = "\n" + instanceCode;
    }

    // Insert the instance code
    auto &instanceNode = parse(instanceCode);
    insertAtBack(module.members, instanceNode);
  }
}

void getAST(const std::string &verilog_file, const std::string &cell) {
  try {
    spdlog::info("Starting get AST from Verilog file: '{}'", verilog_file);
    auto result = slang::syntax::SyntaxTree::fromFile(verilog_file);
    if (result) {
      spdlog::info("Successfully parsed Verilog file.");

      try {
        // First, use the visitor to print basic information
        VerilogVisitor visitor(cell);
        visitor.visit(result.value()->root());

        // If a target cell is specified, use the rewriter to extract the relevant code
        if (!cell.empty()) {
          // Create a rewriter to only keep the target cell related code
          CellExtractor extractor(cell);
          auto extractedTree = extractor.transform(result.value());

          if (extractor.foundTargetCell()) {
            // Use SyntaxPrinter to print the extracted code
            std::string extractedCode = slang::syntax::SyntaxPrinter::printFile(*extractedTree);

            // Save to a file named after the cell name
            std::string outputFile = cell + ".v";
            std::ofstream cellOut(outputFile);
            if (cellOut) {
              cellOut << extractedCode;
              cellOut.close();
              spdlog::info("Extracted '{}' cell code to '{}'", cell, outputFile);
            } else {
              spdlog::error("Failed to write extracted cell code to '{}'", outputFile);
            }
          } else {
            spdlog::warn("Target cell '{}' not found in the Verilog file", cell);
          }
        }

        spdlog::info("Print full source code to 'full_source_code.v'");
        // Optionally save the entire syntax tree
        std::string fullOutput = slang::syntax::SyntaxPrinter::printFile(*result.value());
        std::ofstream out("full_source_code.v");
        out << fullOutput;
        out.close();

        spdlog::info("Print target cell code to 'cell_code.v'");
        // Use CellPrinter to print the target cell's code
        std::ofstream cellOut("cell_code.v");
        CellPrinter cellPrinter(cell, cellOut);
        cellPrinter.visit(result.value()->root());
        cellOut.close();

      } catch (const std::exception &e) {
        spdlog::error("Exception during AST traversal: {}", e.what());
      } catch (...) {
        spdlog::error("Unknown exception during AST traversal");
      }
    } else {
      spdlog::error("Error parsing Verilog file.");
    }
  } catch (const std::exception &e) {
    spdlog::error("Exception during Verilog parsing: {}", e.what());
  } catch (...) {
    spdlog::error("Unknown exception during Verilog parsing");
  }
}