# ZlibValidation

## Description

Command line tool to validate standard cell libraries in `.lib` format.

`ZlibValidation` provides a suite of tools for engineers working with digital IC standard cell libraries in the Liberty format (.lib). It helps ensure library quality, consistency, and facilitates comparison and conversion tasks. Built with C++ for performance and leveraging robust libraries for parsing and command-line interaction.

## Table of Contents

- [ZlibValidation](#zlibvalidation)
  - [Description](#description)
  - [Table of Contents](#table-of-contents)
  - [Motivation](#motivation)
  - [Installation](#installation)
    - [Prerequisites](#prerequisites)
    - [Optional Pre-requisites](#optional-pre-requisites)
    - [Building from Source (Recommended Method)](#building-from-source-recommended-method)
    - [Running ZlibValidation](#running-zlibvalidation)
    - [(Optional) Adding to your PATH](#optional-adding-to-your-path)
  - [Help Message / Features](#help-message--features)
  - [Example Usage](#example-usage)
  - [Documentation and Reference Manual](#documentation-and-reference-manual)
    - [Doumentation Generation](#doumentation-generation)
  - [Acknowledgements](#acknowledgements)
    - [Core Functionality Libraries:](#core-functionality-libraries)
    - [Build, Documentation, and External Tools:](#build-documentation-and-external-tools)


## Motivation

Validating standard cell libraries (.lib) is critical in chip design, but current solutions pose challenges. Commercial tools are expensive, locking out many users (students, startups, researchers), and their closed-source nature prevents customization and hinders innovation. This creates a gap, especially for those needing rapid validation within tight resource constraints.

`ZlibValidation` aims to fill this gap. It is a **free, open-source command-line tool** designed for comprehensive and efficient .lib file validation. Our goals are to:

- Lower the barrier to entry for library quality assurance.
- Provide **fast**, **reliable checks** (parsing, consistency, function, parameters, comparison).
- Enable **transparency**, **customization**, and **community collaboration** through open source.
- Bridge the divide between academic research and industrial needs in validation technology.

## Installation

`ZlibValidation` is designed to be easily built and run directly from source **without requiring administrator** (`sudo`) **privileges** on Linux systems. This makes it ideal for environments where users have restricted permissions. The recommended installation method is building from source.

### Prerequisites

Before you begin, ensure you have the following installed on your **Linux** system:

- **Git:** To clone the repository.
- **C++ Compiler:** C++20 standard support required (e.g., GCC >= 12). Developed with GCC 14.2.0.
- **CMake:** Version 3.25 or higher (versions after 4.0.0 may exhibit compatibility issues).
- **Ninja** (recommended) or **Make**: Build systems. Ninja is preferred for its speed.

### Optional Pre-requisites

- **ccache:** Optional but recommended for speeding up recompilation. Install it via your package manager (e.g., `conda install conda-forge::ccache`).
- **lld:** Optional but recommended for faster linking. Install it via your package manager (e.g., `conda install conda-forge::lld`).

### Building from Source (Recommended Method)

Follow these steps to compile `ZlibValidation` in your user directory:

1. **Clone the repository:**
   ```bash
   git clone https://github.com/Cedar17/ZlibValidation.git
   cd ZlibValidation
   ```

2. **Configure the build using CMake:**
    Create a build directory and run CMake from within it. This keeps build files separate from your source code.
    ```bash
    mkdir build
    cd build
    # Use Ninja as the build system (recommended):
    cmake .. -G Ninja
    # or if you prefer Make:
    cmake .. -G "Unix Makefiles"
    ```
    *Note: No `sudo` is needed here.*

3. **Compile the project:**
    Use `ninja` to build the executable. If you chose Make, use `make` instead, the `-j` flag speeds up compilation by using multiple processor cores.
    ```bash
    # If you used Ninja:
    ninja
    # or if you used Make:
    make -j$(nproc) # Adjust $(nproc) or use a specific number like -j8 for multi-threading
    ```

4. **Locate the Executable:**
   Upon successful compilation, the `zlibvalidation` executable will be located inside the `build` directory: `./build/zlibvalidation`.
   ```bash
   # Check the executable version:
   ./zlibvalidation --version
   ```

### Running ZlibValidation

Since we haven't performed a system-wide install (which would typically require `sudo`), you need to run the executable using its path relative to your current location.

- If you are in the project's root directory (`ZlibValidation/`), run it like this:
  ```bash
  ./build/zlibvalidation --help
  ```
- If you are inside the `build` directory, run it like this:
  ```bash
  ./zlibvalidation --help
  ```

### (Optional) Adding to your PATH

For convenience, you can temporarily add the `build` directory to your shell's `PATH` variable for the current session:

```bash
export PATH="$PWD/build:$PATH" # Assumes you are in the ZlibValidation root directory
# Now you can run it directly:
zlibvalidation -h
```
To make this change permanent, you can add the above line to your shell's configuration file (e.g., `~/.bashrc`, `~/.zshrc`).

## Help Message / Features

```zsh
ZlibValidation
Usage: ./zlibvalidation [OPTIONS] [SUBCOMMAND]

Options:
  -h,--help                   Print this help message and exit
  -v,--version                Display program version information and exit

Subcommands:
  parse                       Parse the Liberty file and write JSON to a file
  mono                        Check the monotonicity of timing arc values
  compare                     Compare the comparison library against the reference one and report differences
  supercell                   Generate supercells for the given Liberty file
  zlibboost                   ZlibBoost - Multi-threaded Library Processing Tool
  clear                       Clear the log, JSON, map, markdown, Verilog, SPICE files in this directory
  verilog                     Generate Verilog file for given Liberty file
  spice                       Generate SPICE file for given Liberty file
  func                        Check functional equivalence of two Liberty files or Verilog files
```

## Example Usage

See [test](test) directory. There are some shell scripts showing how to use `ZlibValidation`. You can modify them to validate your own standard cell library pdk.

## Documentation and Reference Manual

See [https://cedar17.github.io/ZlibValidation/](https://cedar17.github.io/ZlibValidation/) for HTML documentation and PDF reference manual.

### Doumentation Generation

To generate the documentation, you need to have Doxygen and Graphviz installed. You can install them via your package manager or using conda:

```bash
conda install -c conda-forge doxygen graphviz
```

Then, run the following command in the root directory of the project:

```bash
doxygen Doxyfile
```
This will generate the documentation in the `doc_soxygen` directory. You can open the `doc_doxygen\html\index.html` file in your web browser to view the documentation.

Alternatively, you can generate the PDF reference manual by LaTeX.

```bash
cd doc_doxygen\latex
make
```

## Acknowledgements

`ZlibValidation` leverages the power of several excellent third-party libraries, build tools, and external utilities. We extend our sincere gratitude to the developers and communities behind these projects, which were instrumental in building this tool:

### Core Functionality Libraries:

- **[Liberty Parser (csguth/LibertyParser)](https://github.com/csguth/LibertyParser):**
  - **Purpose:** Provides the core functionality for parsing the `.lib` (Liberty) standard cell library format. This open-source implementation forms the basis of our library reading capabilities.
  - **Integration:** The source code was cloned from the repository, manually compiled into a static library (`libsi2dr_liberty.a`), and included directly within this project's `include_3rd_party` directory.
  - **License:** SYNOPSYS Open Source License Version 1.0.

- **[CLI11](https://github.com/CLIUtils/CLI11):**
  - **Purpose:** Enables the robust, feature-rich, and user-friendly command-line interface, handling subcommands and options parsing.
  - **Integration:** Managed via CMake's `FetchContent` mechanism during the build process.
  - **License:** BSD 3-Clause License.

- **[spdlog](https://github.com/gabime/spdlog):**
  - **Purpose:** Provides a highly efficient and flexible library for structured logging throughout the application, critical for debugging and user feedback.
  - **Integration:** Managed via CMake's `FetchContent`.
  - **License:** MIT License.

- **[nlohmann/json](https://github.com/nlohmann/json):**
  - **Purpose:** Used extensively for representing the parsed Liberty library data internally as JSON objects, facilitating data storage, retrieval, and manipulation (e.g., for monotonicity checks and comparisons).
  - **Integration:** Managed via CMake's `FetchContent`.
  - **License:** MIT License.

- **[ZlibBoost](https://github.com/skycrapers/ZlibBoost):**
  - **Purpose:** A multi-threaded library processing tool which is the foundation of the `zlibboost` subcommand. It significantly boost standard cell library characterization with machine learning using Python. See https://doi.org/10.1145/3658617.3703638 for detailed paper.
  - **Integration:** Need to specify the path to the `ZlibBoost` main python script and python environment in the `ZlibValidation` command line.
  - **License:** BSD 3-Clause License with Commercial Use Restriction.

- **[tabulate](https://github.com/p-ranav/tabulate):**
  - **Purpose:** Generates formatted text-based tables, significantly improving the readability of reports generated by the `compare` subcommand.
  - **Integration:** Managed via CMake's `FetchContent`.
  - **License:** MIT License.

- **[slang](https://github.com/MikePopoloski/slang):**
  - **Purpose:** A powerful library for parsing SystemVerilog/Verilog code. It's used here to analyze Verilog netlists, extract Abstract Syntax Trees (AST), generate structural Verilog (`verilog` subcommand), and extract logic for functional equivalence checks (`func` subcommand).
  - **Integration:** Managed via CMake's `FetchContent`.
  - **License:** MIT License.

- **[ExprTk (Expression Toolkit Library)](http://www.partow.net/programming/exprtk/index.html):**
  - **Purpose:** A fast mathematical expression parser and evaluation engine. It is used in the `func` subcommand to dynamically evaluate the logical function strings extracted from libraries or Verilog, enabling the functional equivalence check by comparing truth tables.
  - **Integration:** The header file (`exprtk.hpp`) is included directly within this project's `include_3rd_party` directory.
  - **License:** MIT License.

### Build, Documentation, and External Tools:

- **[CMake](https://cmake.org/)**: The cross-platform build system generator used to configure and manage the entire compilation process.
- **[Doxygen](https://www.doxygen.nl/)** & **[Graphviz](https://graphviz.org/)**: Employed for automatically generating source code documentation and visualizing relationships, aiding development and understanding.
- **[V2LVS (Calibre® Utility)](https://resources.sw.siemens.com/en-US/video-how-to-translate-verilog-netlists-to-spice-using-calibre-v2lvs-utility/):**
  - **Purpose:** A command-line tool included with the Siemens EDA Calibre® platform, designed to translate structural Verilog netlists into a basic SPICE format, typically for Layout Versus Schematic (LVS) verification.
  - **Integration:** The `spice` subcommand **optionally** utilizes `v2lvs` if it's found in the system's PATH. It serves as the first-pass engine to convert the intermediate Verilog representation into a raw SPICE netlist. `ZlibValidation` then performs post-processing on this output, and ensure correct formatting.

The seamless integration facilitated by CMake FetchContent for many of these libraries, combined with the direct inclusion of others, significantly streamlined development and enabled the rich feature set of `ZlibValidation`. We encourage users to consult the individual project licenses for detailed terms of use.
