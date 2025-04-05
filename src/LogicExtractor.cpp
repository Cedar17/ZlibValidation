#include "LogicExtractor.hpp"

// --- Implementation for LogicExtractor ---

/**
 * @brief Handles a module declaration syntax node.
 *
 * This function is the entry point for processing a module declaration. It checks if the module
 * is the target module and, if so, extracts relevant information such as primary inputs,
 * primary outputs, and internal wires. It also visits the children of the module node to
 * process ports, declarations, and instances.
 *
 * The function ensures that the target module is only processed once and that parsing stops
 * after the target module is found.
 *
 * @param module A reference to the module declaration syntax node.
 */
void LogicExtractor::handle(const slang::syntax::ModuleDeclarationSyntax &module) {
  if (parsingComplete_)
    return; // Don't re-process if called again

  if (module.header && module.header->name.valid()) {
    std::string_view moduleName = module.header->name.valueText();
    if (!targetCell_.empty() && moduleName == targetCell_) {
      spdlog::info("LogicExtractor: Found target module: {}", targetCell_);
      inTargetModule_ = true;

      // Reset state for the target module
      primaryInputs_.clear();
      primaryOutputs_.clear();
      internalWires_.clear();
      portDirections_.clear(); // Clear temporary direction map
      gateOutputDrivers_.clear();
      // logicCache_.clear(); // For Step 2

      // Visit children (ports, declarations, instances) IN ORDER
      // It's often better to visit declarations first, then the port list
      // Slang's default visitDefault might handle this, but be aware.
      visitDefault(module);

      // --- Finalize Ports after visiting declarations and port list ---
      // This part might be better placed *after* visiting NonAnsiPortList,
      // assuming the list visit confirms the names from the header.
      // Let's move the finalization logic to handle(NonAnsiPortListSyntax)

      // Mark that we finished processing the target module
      inTargetModule_ = false; // Important to prevent processing other modules
      parsingComplete_ = true; // Stop processing after finding the target
      spdlog::info("LogicExtractor: Finished visiting target module '{}'.", targetCell_);

    } else if (!parsingComplete_) {
      // If not the target module yet, continue searching
      visitDefault(module);
    }
  } else if (!parsingComplete_) {
    // Handle modules without valid headers if necessary, or just traverse
    visitDefault(module);
  }
}

// Handle Port Declarations (defines direction and type, usually inside module body)
/**
 * @brief Handles a port declaration syntax node.
 *
 * This method extracts information about port declarations within a SystemVerilog module,
 * including the port's direction (input, output, or inout) and name. It updates internal
 * data structures to track primary inputs, primary outputs, internal wires, and port directions.
 *
 * @param portDecl A reference to the PortDeclarationSyntax node being processed.
 *
 * @details
 * - It first checks if the current processing context is within the target module. If not, the
 *   method returns early.
 * - It determines the port's direction by inspecting the port header (VariablePortHeaderSyntax or
 *   NetPortHeaderSyntax). The direction is extracted using `valueText()` to handle keywords
 *   represented as text tokens.
 * - It iterates through the declarators in the port declaration to extract port names.
 * - If multiple direction declarations are found for the same port, a warning is logged, and the
 *   first declared direction is retained.
 * - Based on the port's direction, the port name is added to the appropriate sets:
 *   - `primaryInputs_`: For input and inout ports.
 *   - `primaryOutputs_`: For output and inout ports.
 *   - `internalWires_`: For all ports (input, output, inout, and ports with unknown directions).
 * - If a port has an unknown or missing direction, it's treated as an internal wire, and a warning
 *   is logged.
 * - The method avoids calling `visitDefault` to prevent unexpected revisits of syntax nodes.
 *
 * @note The `logicCache_` update is commented out, indicating it's part of a later processing step.
 * @warning Logs warnings for port declarations without headers, port declarators without names, and
 *          multiple direction declarations for the same port.
 */
void LogicExtractor::handle(const slang::syntax::PortDeclarationSyntax &portDecl) {
  if (!inTargetModule_)
    return;
  spdlog::debug("LogicExtractor: Handling Port Declaration");

  std::string direction = "unknown";
  if (portDecl.header) {
    // Determine direction (input/output/inout)
    // Important: Use valueText() as Slang typically represents keywords as text tokens
    if (auto varHeader = portDecl.header->as_if<slang::syntax::VariablePortHeaderSyntax>()) {
      if (varHeader->direction.valid()) {
        direction = std::string(varHeader->direction.valueText());
      }
    } else if (auto netHeader = portDecl.header->as_if<slang::syntax::NetPortHeaderSyntax>()) {
      if (netHeader->direction.valid()) {
        direction = std::string(netHeader->direction.valueText());
      }
    }
    // Add InterfacePortHeaderSyntax if needed
  } else {
    spdlog::warn("LogicExtractor: Port declaration without header found.");
  }

  for (const auto &declarator : portDecl.declarators) {
    if (declarator->name.valid()) {
      std::string portName = std::string(declarator->name.valueText());
      if (portDirections_.count(portName) && portDirections_[portName] != "unknown") {
        spdlog::warn("LogicExtractor: Multiple direction declarations for port '{}'. Keeping first "
                     "one ('{}'). New direction: '{}'",
                     portName, portDirections_[portName], direction);
      } else {
        spdlog::info("LogicExtractor: Storing direction '{}' for port '{}'", direction, portName);
        portDirections_[portName] = direction;
        if (direction == "input") {
          primaryInputs_.insert(portName);
          internalWires_.insert(portName); // Inputs are also technically 'wires' usable internally
        } else if (direction == "output") {
          primaryOutputs_.insert(portName);
          internalWires_.insert(portName); // Outputs are also wires driven by something
        } else if (direction == "inout") {
          spdlog::info("LogicExtractor: Inout port '{}' found. Adding to both inputs and outputs.",
                       portName);
          primaryInputs_.insert(portName);
          primaryOutputs_.insert(portName);
          internalWires_.insert(portName);
        } else {
          spdlog::warn("LogicExtractor: Port '{}' found in declaration but has unknown or missing "
                       "direction '{}'. Treating as internal wire.",
                       portName, direction);
          internalWires_.insert(portName);
        }
        // logicCache_[portName] = portName; // For Step 2
      }
    } else {
      spdlog::warn(
          "LogicExtractor: Port declarator without a name found in PortDeclarationSyntax.");
    }
  }
  // Don't call visitDefault here, as it might revisit things unexpectedly.
  // Let the main module visitor handle traversing into children if necessary.
}

// Handle Non-ANSI Port List (defines names, usually in module header)
/**
 * @brief Handles a non-ANSI port list in a module.
 *
 * This method iterates through the ports in a non-ANSI port list, extracts the port name,
 * determines its direction (input, output, or inout), and adds it to the appropriate sets
 * (primaryInputs_, primaryOutputs_, internalWires_). It also handles different types of
 * port declarations, including implicit, explicit, and empty ports.
 *
 * @param portList A reference to the NonAnsiPortListSyntax object representing the port list.
 *
 * @details
 * - Skips processing if not in the target module (inTargetModule_ is false).
 * - Extracts port names from ImplicitNonAnsiPortSyntax and ExplicitNonAnsiPortSyntax.
 * - Handles PortConcatenationSyntax by logging a warning and skipping the port.
 * - Skips EmptyNonAnsiPortSyntax (placeholders).
 * - Uses the portDirections_ map to determine the direction of each port.
 * - Adds ports to primaryInputs_ and internalWires_ if the direction is "input".
 * - Adds ports to primaryOutputs_ and internalWires_ if the direction is "output".
 * - Adds ports to both primaryInputs_ and primaryOutputs_ if the direction is "inout".
 * - Logs warnings for ports with unknown or missing directions.
 * - Logs warnings if the port name cannot be determined.
 *
 * @note This method does not call visitDefault.
 */
void LogicExtractor::handle(const slang::syntax::NonAnsiPortListSyntax &portList) {
  if (!inTargetModule_)
    return;
  spdlog::debug("LogicExtractor: Handling NonAnsi Port List");

  for (const auto portSyntax : portList.ports) {
    if (!portSyntax)
      continue;

    std::string portName = "";
    // Most common case: ImplicitNonAnsiPortSyntax contains the expression (usually just the name)
    if (auto implicitPort = portSyntax->as_if<slang::syntax::ImplicitNonAnsiPortSyntax>()) {
      if (implicitPort->expr) {
        // The expression itself might be complex, try to get the simple name
        if (auto portRef = implicitPort->expr->as_if<slang::syntax::PortReferenceSyntax>()) {
          if (portRef->name.valid()) {
            portName = std::string(portRef->name.valueText());
          }
        } else if (auto portConcat =
                       implicitPort->expr->as_if<slang::syntax::PortConcatenationSyntax>()) {
          // Handle concatenations if necessary - complex
          spdlog::warn("LogicExtractor: Port concatenation found in NonAnsi port list - currently "
                       "not fully handled for logic extraction. Port: {}",
                       implicitPort->toString());
          continue; // Skip complex ports for now
        } else {
          // Fallback: try getting name from the expression directly (might be just identifier)
          portName = implicitPort->toString();
        }
      }
    }
    // Handle ExplicitNonAnsiPortSyntax (e.g., .A(A)) if needed
    else if (auto explicitPort = portSyntax->as_if<slang::syntax::ExplicitNonAnsiPortSyntax>()) {
      if (explicitPort->name.valid()) {
        portName = std::string(explicitPort->name.valueText());
        // Note: explicitPort->expr is the internal signal it connects to.
        // We are primarily interested in the port's own name (explicitPort->name) here.
      }
    }
    // Handle EmptyNonAnsiPortSyntax (commas for placeholders) if necessary
    else if (portSyntax->kind == slang::syntax::SyntaxKind::EmptyNonAnsiPort) {
      spdlog::debug("LogicExtractor: Skipping empty non-ANSI port placeholder.");
      continue;
    } else {
      spdlog::warn("LogicExtractor: Unhandled NonAnsi port syntax kind: {}",
                   slang::syntax::toString(portSyntax->kind));
      continue;
    }

    // Now we (hopefully) have the portName.
    if (!portName.empty()) {
      if (portDirections_.count(portName)) {
        const std::string &direction = portDirections_[portName];
        spdlog::debug("LogicExtractor: Finalizing port '{}' with direction '{}'", portName,
                      direction);

        if (direction == "input") {
          primaryInputs_.insert(portName);
          internalWires_.insert(portName); // Inputs are also technically 'wires' usable internally
                                           // logicCache_[portName] = portName; // For Step 2
        } else if (direction == "output") {
          primaryOutputs_.insert(portName);
          internalWires_.insert(portName); // Outputs are also wires driven by something
        } else if (direction == "inout") {
          spdlog::info("LogicExtractor: Inout port '{}' found. Adding to both inputs and outputs.",
                       portName);
          primaryInputs_.insert(portName);
          primaryOutputs_.insert(portName);
          internalWires_.insert(portName);
          // logicCache_[portName] = portName; // For Step 2
        } else {
          spdlog::warn("LogicExtractor: Port '{}' found in list but has unknown or missing "
                       "direction '{}'. Treating as internal wire.",
                       portName, direction);
          internalWires_.insert(portName);
        }
      } else {
        spdlog::debug("LogicExtractor: Port '{}' found in NonAnsi list, no direction declaration ",
                      portName);
      }
    } else {
      spdlog::warn("LogicExtractor: Could not determine port name from NonAnsi port list item: {}",
                   portSyntax->toString());
    }
  }
  // Don't call visitDefault here
}

// Handle explicit wire declarations
/**
 * @brief Handles a net declaration syntax node.
 *
 * This function processes a net declaration syntax node, extracting the names of declared wires.
 * It checks if the current scope is within the target module and avoids adding ports again if they
 * were already declared as nets. The extracted wire names are added to the internal wires set.
 *
 * @param netDecl A reference to the NetDeclarationSyntax node to handle.
 */
void LogicExtractor::handle(const slang::syntax::NetDeclarationSyntax &netDecl) {
  if (!inTargetModule_)
    return;

  for (const auto &declarator : netDecl.declarators) {
    if (declarator->name.valid()) {
      std::string wireName = std::string(declarator->name.valueText());
      spdlog::debug("LogicExtractor: Found wire declaration: {}", wireName);
      // Avoid adding ports again if they were also declared as nets (common)
      if (!primaryInputs_.count(wireName) && !primaryOutputs_.count(wireName)) {
        internalWires_.insert(wireName);
      } else {
        spdlog::trace("LogicExtractor: Wire '{}' is already known as a port.", wireName);
      }
    }
  }
  // Don't call visitDefault here
}

// Handle primitive gate instantiations (MOST IMPORTANT PART)
/**
 * @brief Handles the extraction of logic gate information from a primitive instantiation syntax node.
 *
 * This method processes a primitive instantiation, extracting the gate type, input signals, and output signal.
 * It populates the `gateOutputDrivers_` map, which stores the driving gate information for each output signal.
 * It also identifies internal wires and checks for multiple drivers on the same signal.
 *
 * @param primitiveInst A reference to the PrimitiveInstantiationSyntax node representing the gate instance.
 */
void LogicExtractor::handle(const slang::syntax::PrimitiveInstantiationSyntax &primitiveInst) {
  if (!inTargetModule_)
    return;

  if (!primitiveInst.type.valid()) {
    spdlog::warn("LogicExtractor: Primitive instance without a type token found.");
    return;
  }

  // Use token kind for reliable checking, text for name storage
  slang::parsing::TokenKind gateKind = primitiveInst.type.kind;
  std::string gateTypeName = std::string(primitiveInst.type.valueText());

  spdlog::debug("LogicExtractor: Found Primitive Instance of Type: {} (Kind: {})", gateTypeName,
                slang::parsing::toString(gateKind));

  for (const auto &instance : primitiveInst.instances) {
    // if (!instance || !instance->connections.isInitialized()) { // Check if connections are valid
    //   spdlog::warn("LogicExtractor: Skipping primitive instance of type {} due to null pointer or
    //   "
    //                "uninitialized connections.",
    //                gateTypeName);
    //   continue;
    // }

    GateInfo currentGateInfo;
    currentGateInfo.gateTypeName = gateTypeName;
    currentGateInfo.kind = gateKind;

    // Primitives usually have ordered connections.
    // The FIRST connection is typically the OUTPUT.
    // The REST are INPUTS.
    if (instance->connections.empty()) {
      spdlog::warn("LogicExtractor: Gate instance of type {} has no connections.", gateTypeName);
      continue;
    }

    // Extract Output Signal
    // Ensure the connection itself is not null
    if (!instance->connections[0]) {
      spdlog::error("LogicExtractor: First connection (output) is null for primitive {}",
                    gateTypeName);
      continue;
    }
    if (auto firstConn =
            instance->connections[0]->as_if<slang::syntax::OrderedPortConnectionSyntax>()) {
      currentGateInfo.outputSignal =
          firstConn->expr->toString(); // Get the output signal name from the connection
                                       // remove extra spaces in the signal name
      currentGateInfo.outputSignal.erase(
          std::remove_if(currentGateInfo.outputSignal.begin(), currentGateInfo.outputSignal.end(),
                         [](unsigned char x) { return std::isspace(x); }),
          currentGateInfo.outputSignal.end());
      if (!currentGateInfo.outputSignal.empty()) {
        spdlog::debug("  Output Signal: {}", currentGateInfo.outputSignal);
        // Gate outputs are internal signals (unless they are module outputs)
        // Add to internal wires if not already a primary output.
        if (!primaryOutputs_.count(currentGateInfo.outputSignal)) {
          internalWires_.insert(currentGateInfo.outputSignal);
        }
      } else {
        spdlog::error(
            "LogicExtractor: Could not determine output signal name for gate instance of type {}",
            gateTypeName);
        continue; // Skip this instance if output is unknown
      }
    } else {
      spdlog::error("LogicExtractor: Expected OrderedPortConnectionSyntax for output of primitive "
                    "{}, but got kind: {}. Conn: {}",
                    gateTypeName, slang::syntax::toString(instance->connections[0]->kind),
                    instance->connections[0]->toString());
      continue; // Skip this instance
    }

    // Extract Input Signals
    for (size_t i = 1; i < instance->connections.size(); ++i) {
      // Ensure the connection itself is not null
      if (!instance->connections[i]) {
        spdlog::warn("LogicExtractor: Input connection {} is null for primitive {}", i,
                     gateTypeName);
        continue;
      }
      if (auto conn =
              instance->connections[i]->as_if<slang::syntax::OrderedPortConnectionSyntax>()) {
        std::string inputSig =
            conn->expr->toString(); // Get the input signal name from the connection
        // remove extra spaces in the signal name
        inputSig.erase(std::remove_if(inputSig.begin(), inputSig.end(),
                                      [](unsigned char x) { return std::isspace(x); }),
                       inputSig.end());
        if (!inputSig.empty()) {
          currentGateInfo.inputSignals.push_back(inputSig);
          spdlog::debug("  Input Signal {}: {}", i, inputSig);
        } else {
          spdlog::warn("LogicExtractor: Could not determine input signal name for input {} of gate "
                       "instance type {}. Conn: {}",
                       i, gateTypeName, conn->toString());
          // Decide whether to skip or continue with partial inputs
        }
      } else {
        spdlog::warn("LogicExtractor: Expected OrderedPortConnectionSyntax for input {} of "
                     "primitive {}, but got kind: {}. Conn: {}",
                     i, gateTypeName, slang::syntax::toString(instance->connections[i]->kind),
                     instance->connections[i]->toString());
      }
    }

    // Store the gate information, mapping the output signal to its driving gate
    if (!currentGateInfo.outputSignal.empty()) {
      if (gateOutputDrivers_.count(currentGateInfo.outputSignal)) {
        // This is a critical warning - indicates multiple drivers for the same net!
        spdlog::error("LogicExtractor: Multiple drivers found for signal '{}'! Previous driver: "
                      "{}, New driver: {}. Netlist is likely invalid.",
                      currentGateInfo.outputSignal,
                      gateOutputDrivers_[currentGateInfo.outputSignal].gateTypeName, gateTypeName);
        // Keep the first one found for now, or decide on error handling
      } else {
        spdlog::info("  Storing driver for '{}': Gate Type '{}'", currentGateInfo.outputSignal,
                     gateTypeName);
        gateOutputDrivers_[currentGateInfo.outputSignal] = currentGateInfo;
      }
    }
  }
  // Don't call visitDefault here
}

// --- Logic Derivation Implementation ---

// Public method called after visiting the tree
/**
 * @brief Extracts logic expressions for all primary output ports of the target cell.
 *
 * This method iterates through the primary output ports, derives the logic expression
 * for each, and stores the result in a map. If an error occurs during the derivation
 * of logic for a particular output, an error message is stored in the map instead.
 *
 * @return A map where the key is the output port name (std::string) and the value is the
 *         corresponding logic expression (std::string).  If AST parsing is not complete
 *         or the target module is not found, an empty map is returned.  If an error
 *         occurs during logic derivation for a specific output, the corresponding value
 *         in the map will be an error message.
 */
std::map<std::string, std::string> LogicExtractor::getLogicExpressions() {
  std::map<std::string, std::string> result_map;
  if (!parsingComplete_) {
    spdlog::error("LogicExtractor: AST parsing did not complete or target module '{}' not found. "
                  "Cannot extract logic.",
                  targetCell_);
    return result_map; // Return empty map
  }

  spdlog::info("LogicExtractor: Deriving logic expressions for {} output ports...",
               primaryOutputs_.size());

  for (const std::string &outputPort : primaryOutputs_) {
    spdlog::debug("LogicExtractor: Deriving logic for output: {}", outputPort);
    try {
      result_map[outputPort] = deriveLogicRecursive(outputPort);
      spdlog::info("  Output: {} => {}", outputPort, result_map[outputPort]);
    } catch (const std::runtime_error &e) {
      spdlog::error("LogicExtractor: Error deriving logic for output '{}': {}", outputPort,
                    e.what());
      result_map[outputPort] = "/* Error deriving logic */";
    } catch (...) {
      spdlog::error("LogicExtractor: Unknown error deriving logic for output '{}'", outputPort);
      result_map[outputPort] = "/* Unknown error deriving logic */";
    }
  }
  return result_map;
}

// Recursive function with memoization
/**
 * @brief Recursively derives the logic expression for a given signal.
 *
 * This function traces back the signal to its driving gates and primary inputs,
 * constructing a logic expression that represents the signal's behavior. It uses
 * memoization to cache previously computed expressions, avoiding redundant calculations.
 *
 * @param signalName The name of the signal for which to derive the logic expression.
 * @return A string representing the logic expression for the signal.
 * @throws std::runtime_error if the signal is not a primary input, a known wire,
 *                            or driven by a recognized gate/assignment. Also thrown if an
 *                            empty input signal name is encountered or if an internal wire
 *                            has no identified driver.
 *
 * @details
 * The function operates in the following steps:
 * 1. **Check Cache (Memoization):** If the logic expression for the signal is already
 *    cached in `logicCache_`, it is immediately returned.
 * 2. **Base Case: Is it a primary input?** If the signal is a primary input
 *    (present in `primaryInputs_`), its logic expression is simply its own name.
 * 3. **Recursive Step: Is it driven by a gate?** If the signal is driven by a gate
 *    (present in `gateOutputDrivers_`), the function recursively derives the logic
 *    expressions for all input signals of the gate. These input expressions are then
 *    used to format the overall expression for the current signal based on the gate type.
 * 4. **Handle Assign statements:**  (Currently not implemented but reserved for future use).
 * 5. **Error Case:** If the signal is not found in any of the above categories, it indicates
 *    an error. An exception is thrown, indicating that the signal is either an undriven
 *    internal wire or an unknown signal.
 */
std::string LogicExtractor::deriveLogicRecursive(const std::string &signalName) {
  // 1. Check Cache (Memoization)
  if (logicCache_.count(signalName)) {
    return logicCache_.at(signalName);
  }

  // 2. Base Case: Is it a primary input?
  if (primaryInputs_.count(signalName)) {
    // Already cached during port handling, but double-check
    if (!logicCache_.count(signalName)) {
      logicCache_[signalName] = signalName;
    }
    return signalName;
  }

  // 3. Recursive Step: Is it driven by a gate?
  if (gateOutputDrivers_.count(signalName)) {
    const GateInfo &driverGate = gateOutputDrivers_.at(signalName);
    std::vector<std::string> inputExpressions;
    spdlog::debug("    Tracing signal '{}', driven by {} gate", signalName,
                  driverGate.gateTypeName);

    // Recursively find expressions for all inputs of this gate
    for (const std::string &inputSig : driverGate.inputSignals) {
      if (inputSig.empty()) {
        throw std::runtime_error("Empty input signal name encountered for gate driving " +
                                 signalName);
      }
      spdlog::debug("      Recursing for input: {}", inputSig);
      inputExpressions.push_back(deriveLogicRecursive(inputSig));
    }

    // Format the expression based on gate type and input expressions
    std::string currentExpr = formatExpression(driverGate, inputExpressions);

    // Cache the result
    logicCache_[signalName] = currentExpr;
    return currentExpr;
  }

  // 4. Handle Assign statements (if implemented)
  // if (assignDrivers_.count(signalName)) { ... }

  // 5. Error Case: Signal not found or not driven by known element
  // Check if it's just an internal wire that wasn't driven?
  if (internalWires_.count(signalName)) {
    throw std::runtime_error(
        "Signal '" + signalName +
        "' is an internal wire but has no identified driver (gate or assign).");
  } else {
    throw std::runtime_error(
        "Signal '" + signalName +
        "' is not a primary input, known wire, or driven by a recognized gate/assignment.");
  }
}

// Helper to format the expression string based on gate type
/**
 * @brief Formats a logic expression based on the gate type and input expressions.
 *
 * This function takes a GateInfo structure describing the gate and a vector of input
 * expressions (strings) and constructs a logic expression string representing the gate's
 * operation. It handles AND, NAND, OR, NOR, XOR, XNOR, NOT, and BUF gates.  It uses
 * '*', '+', '^', and '!' operators for AND, OR, XOR, and NOT respectively.  If the gate
 * type is unsupported, or if the number of inputs is incorrect for NOT/BUF gates, or if
 * a gate requiring inputs receives none, an error string is returned, and a warning is
 * logged.
 *
 * @param gateInfo A GateInfo struct containing information about the gate, including its type
 *                 (slang::parsing::TokenKind) and name.
 * @param inputExprs A vector of strings representing the input expressions to the gate.
 * @return A string representing the formatted logic expression, or an error string if
 *         formatting fails due to unsupported gate type or incorrect number of inputs.
 */
std::string LogicExtractor::formatExpression(const GateInfo &gateInfo,
                                             const std::vector<std::string> &inputExprs) {
  if (inputExprs.empty() && gateInfo.kind != slang::parsing::TokenKind::NotKeyword &&
      gateInfo.kind != slang::parsing::TokenKind::BufKeyword) {
    // Gates like AND/OR/XOR need inputs
    spdlog::warn("Gate type {} requires inputs, but none were provided/derived for output {}",
                 gateInfo.gateTypeName, gateInfo.outputSignal);
    return "/*<Error: Missing Inputs for " + gateInfo.gateTypeName + ">*/";
  }

  std::string result = "";

  // Use gateInfo.type (enum) for reliable checking
  switch (gateInfo.kind) {
  case slang::parsing::TokenKind::AndKeyword:
    result = "(" + inputExprs[0];
    for (size_t i = 1; i < inputExprs.size(); ++i)
      result += " * " + inputExprs[i]; // Use * for AND
    result += ")";
    break;
  case slang::parsing::TokenKind::NandKeyword:
    result = "!(" + inputExprs[0];
    for (size_t i = 1; i < inputExprs.size(); ++i)
      result += " * " + inputExprs[i];
    result += ")";
    break;
  case slang::parsing::TokenKind::OrKeyword:
    result = "(" + inputExprs[0];
    for (size_t i = 1; i < inputExprs.size(); ++i)
      result += " + " + inputExprs[i]; // Use + for OR
    result += ")";
    break;
  case slang::parsing::TokenKind::NorKeyword:
    result = "!(" + inputExprs[0];
    for (size_t i = 1; i < inputExprs.size(); ++i)
      result += " + " + inputExprs[i];
    result += ")";
    break;
  case slang::parsing::TokenKind::XorKeyword:
    result = "(" + inputExprs[0];
    for (size_t i = 1; i < inputExprs.size(); ++i)
      result += " ^ " + inputExprs[i]; // Use ^ for XOR
    result += ")";
    break;
  case slang::parsing::TokenKind::XnorKeyword:
    result = "!(" + inputExprs[0];
    for (size_t i = 1; i < inputExprs.size(); ++i)
      result += " ^ " + inputExprs[i];
    result += ")";
    break;
  case slang::parsing::TokenKind::NotKeyword: // NOT gate
    if (inputExprs.size() != 1) {
      spdlog::warn("NOT gate expects 1 input, got {}", inputExprs.size());
      return "/*<Error: Incorrect Inputs for NOT>*/";
    }
    result = "!" + inputExprs[0]; // Use ! for NOT
    break;
  case slang::parsing::TokenKind::BufKeyword: // BUF gate
    if (inputExprs.size() != 1) {
      spdlog::warn("BUF gate expects 1 input, got {}", inputExprs.size());
      return "/*<Error: Incorrect Inputs for BUF>*/";
    }
    result = inputExprs[0]; // Output is the same as input
    break;
  // Add cases for bufif0, bufif1, notif0, notif1, pullup, pulldown, cmos, nmos, pmos, tran etc. if
  // needed
  default:
    spdlog::warn("Unsupported primitive gate type for logic expression generation: {}",
                 gateInfo.gateTypeName);
    result = "/*<Unsupported Gate: " + gateInfo.gateTypeName + ">*/";
    break;
  }
  return result;
}

// --- New Function Implementation (Step 1: Visit and Print) ---

/**
 * @brief Extracts and prints netlist information from a Verilog file for a specified cell.
 *
 * This function parses a Verilog file using the slang library, extracts information about
 * the primary inputs, primary outputs, internal wires, and gate drivers within a specified
 * cell (module). It then prints a summary of the extracted information to the console using spdlog.
 *
 * @param verilog_file The path to the Verilog file to be parsed.
 * @param cell The name of the cell (module) for which to extract netlist information.
 *
 * @exception std::exception If any error occurs during Verilog parsing or info extraction.
 *
 * @note The function uses the slang library for Verilog parsing and a custom LogicExtractor
 *       class to extract the desired information.  Error messages are logged using spdlog.
 */
void extractAndPrintNetlistInfo(const std::string &verilog_file, const std::string &cell) {
  spdlog::info("--- Step 1: Starting Netlist Info Extraction ---");
  spdlog::info("Verilog file: '{}', Target cell: '{}'", verilog_file, cell);

  try {
    auto result = slang::syntax::SyntaxTree::fromFile(verilog_file);
    if (!result) { // Check if result is valid
      spdlog::error("Error parsing Verilog file '{}'. Cannot extract info.", verilog_file);
      return;
    }

    spdlog::info("Successfully parsed Verilog file.");
    std::shared_ptr<slang::syntax::SyntaxTree> tree = result.value();

    LogicExtractor extractor(cell);
    tree->root().visit(extractor); // Populate extractor's internal state

    // --- Print Summary (Debug for Step 1) ---
    spdlog::info("--- Extraction Summary for Cell '{}': ---", cell);

    const auto &inputs = extractor.getPrimaryInputs();
    spdlog::info("Found {} Primary Inputs:", inputs.size());
    for (const auto &name : inputs) {
      spdlog::info("  - {}", name);
    }

    const auto &outputs = extractor.getPrimaryOutputs();
    spdlog::info("Found {} Primary Outputs:", outputs.size());
    for (const auto &name : outputs) {
      spdlog::info("  - {}", name);
    }

    const auto &wires = extractor.getInternalWires();
    spdlog::info("Found {} Internal Wires:", wires.size());
    for (const auto &name : wires) {
      spdlog::info("  - {}", name);
    }

    const auto &gates = extractor.getExtractedGates();
    spdlog::info("Found {} Gate Drivers:", gates.size());
    for (const auto &pair : gates) {
      const std::string &outputNet = pair.first;
      const GateInfo &info = pair.second;
      std::string inputsStr = "";
      for (size_t i = 0; i < info.inputSignals.size(); ++i) {
        inputsStr += info.inputSignals[i] + (i == info.inputSignals.size() - 1 ? "" : ", ");
      }
      spdlog::info("  - Output: {} <= Driven by: {} ({}) Inputs: [{}]", outputNet,
                   info.gateTypeName, slang::parsing::toString(info.kind), inputsStr);
    }
    spdlog::info("--- End Netlist Info Extraction ---");

  } catch (const std::exception &e) {
    spdlog::error("Exception during Verilog parsing or info extraction: {}", e.what());
  } catch (...) {
    spdlog::error("Unknown exception during Verilog parsing or info extraction.");
  }
}

// --- (Step 2: Extract Logic Expressions) ---

/**
 * @brief Extracts logic expressions from a Verilog file for a specified cell.
 *
 * This function parses a Verilog file using the slang library, identifies the specified cell,
 * and extracts the logic expressions for its outputs. It returns a map where the keys are
 * output signal names and the values are their corresponding logic expressions as strings.
 *
 * @param verilog_file The path to the Verilog file to parse.
 * @param cell The name of the cell (module) for which to extract logic expressions.
 * @return A map of output signal names to their logic expressions. Returns an empty map if parsing fails,
 *         the cell is not found, or no logic expressions can be derived.
 *
 * @note The function uses the slang library for Verilog parsing. Ensure that slang is properly
 *       installed and configured before using this function.
 * @note The logic extraction process involves traversing the syntax tree of the Verilog code
 *       and identifying relevant assignments and expressions within the specified cell.
 * @note Error messages and warnings are logged using the spdlog library.
 */
std::map<std::string, std::string> extractLogicFromVerilog(const std::string &verilog_file,
                                                           const std::string &cell) {
  spdlog::info("--- Starting Logic Expression Extraction for cell: '{}' ---", cell);
  std::map<std::string, std::string> logicMap; // Default empty map

  try {
    auto result = slang::syntax::SyntaxTree::fromFile(verilog_file);
    if (!result) {
      spdlog::error("Error parsing Verilog file '{}'. Cannot extract logic.", verilog_file);
      return logicMap;
    }

    spdlog::info("Successfully parsed Verilog file.");
    std::shared_ptr<slang::syntax::SyntaxTree> tree = result.value();

    LogicExtractor extractor(cell);
    tree->root().visit(extractor); // Populate extractor's internal state

    // Now, call the method to derive and get the expressions
    logicMap = extractor.getLogicExpressions();

  } catch (const std::exception &e) {
    spdlog::error("Exception during Verilog parsing or logic extraction: {}", e.what());
  } catch (...) {
    spdlog::error("Unknown exception during Verilog parsing or logic extraction.");
  }

  if (logicMap.empty()) {
    spdlog::warn("Logic extraction finished, but no expressions were derived for cell '{}'. Check "
                 "if cell exists and is correctly defined.",
                 cell);
  } else {
    spdlog::info("Logic extraction completed for cell '{}'. Found {} output expression strings",
                 cell, logicMap.size());
  }

  spdlog::info("--- End Logic Expression Extraction ---");
  return logicMap;
}
