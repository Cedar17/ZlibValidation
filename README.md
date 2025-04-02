# ZlibValidation

## Description

Command line tool to validate standard cell libraries in `.lib` format.

## Usage

```zsh
ZlibValidation
Usage: ./zlibvalidation [OPTIONS] [SUBCOMMAND]

Options:
  -h,--help                   Print this help message and exit
  -v,--version                Display program version information and exit

Subcommands:
  parse                       Parse the Liberty file and write JSON to a file
  mono                        Check the monotonicity of timing arc values
  compare                     Compare the comparison library against the reference library and report differences
  supercell                   Generate supercells for the given Liberty file
  zlibboost                   ZlibBoost - Multi-threaded Library Processing Tool
  clear                       Clear the log, JSON, map, markdown files in this directory
  verilog                     Generate Verilog file for given Liberty file
  spice                       Generate SPICE file for given Liberty file
  func                        Check functional equivalence of two Liberty files or Verilog files
```

## Development Diary

### 2025-01-27

- 创建了一个C++空项目，使用CMake管理编译链，并手动添加了外部库 `libsi2dr_liberty.a`。
- 添加了 `CLI11` 库以处理命令行参数解析。
- 添加了 `spdlog` 日志库，实现高效的日志格式化输出。

### 2025-01-28

- 添加 `nlohmann/json` 库用于 JSON 数据存储和检索。
- 在 `version.h` 中存储项目作者、版本、构建日期等信息，由 `CMake` 自动维护。
- 简化了 `--version` 参数解析的代码实现，并添加了 `--mode` 选项以选择工作模式。
- 使用 `spdlog` 库的日志记录器将 debug 级别及以上的日志输出到文件，将 info 级别及以上的日志输出到控制台。
- 将库文件顶层封装为`LibFile`对象，提供了读取、解析和修改库文件的方法，包含名称和 PVT 信息作为公共属性。
  - 但仍然使用过程化方法循环迭代读取库文件名称和 PVT 信息。
- 按照 C++ 标准库、第三方库和本地库的顺序重新排列了 `#include` 语句。

### 2025-02-01

- 使用面向对象编程（OOP）重构了原先用C语言循环遍历的代码，将简单属性的读取封装为`LibAttribute`类，
  - 并将函数返回的`char *`类型的属性值转换为`std::string`类型，便于外部使用。
- 将属性迭代过程封装为`AttributesIterator`类，在析构函数中处理迭代退出，实现资源获取即初始化（RAII）。

### 2025-02-10

- 将组的迭代过程封装为`GroupsIterator`类，同样在析构函数中处理迭代退出。
- 封装了`LibGroup`类，提供了读取组名、类别、属性、子组的方法。
- 将类的定义和实现分离，将类的实现放在`src`目录下，将类的定义放在`include`目录下，修改了`CMakeLists.txt`自动收集文件，便于模块化编译。

### 2025-02-11

- 类的头文件改为`hpp`后缀，修改了`CMakeLists.txt`中的文件收集规则，CMake编译时间戳更新。
- 为`LibFile`类添加了输出同名json格式文件的方法`LibFile::writeJsonToFile`，目前能输出PVT、cell_name、cell_footprint、cell_area信息。
- [x] voltage 浮点数误差未解决。

### 2025-02-14

- 在 `json_utils.cpp` 中，实现了以 JSON 数据结构循环迭代存储库 (lib) 信息的功能，具体包括 `generateCellJson, generatePinJson, generatePowerJson, generateLutJson` 等函数，并添加了同名的 hpp 头文件。
- [x] 时序 (timing) 信息解析功能待完成。
- 新增辅助函数 `parseStringToVector`，该函数可将以逗号分隔的字符串解析为浮点数向量，以便于读取 Look Up Table (LUT)。
- 为 `LibFile` 对象添加了私有属性 `lib_json_`，用于存储库 (lib) 的 JSON 对象。
- 封装了用于迭代复杂属性值的 `ValuesIterator` 类。
- 修改了 `LibAttribute::isComplex()` 函数的返回值类型，由原类型变更为 `bool` 类型。
- 重写了 `LibGroup::getName()` 函数，使其直接返回 `std::string` 类型的组名字。
- 重构了迭代器使用代码。移除了中间变量类似`si2drGroupsIdT sub_groups` 的声明步骤。直接使用 `lib_group.getGroups()`的返回值进行初始化`GroupsIterator`，提升了代码的可读性和简洁性。

### 2025-02-15

- 实现`generateTimingJson`函数，用于全面解析时序信息。

### 2025-02-17

- 简化了迭代器在外部的调用流程，删除所有迭代器的`begin()`方法，改为在构造函数内部初始化。

### 2025-02-18

- 新建`LibFile::mono()`方法，准备实现库文件输出引脚的values单调递增性检查。
- 修改了`setupLogger()`函数，将日志文件名可作为参数传入，便于根据不同工作模式切换日志文件。

### 2025-02-19

- 修改了`mode == "mono"`情况下的逻辑，使用 C++17 引入的 `std::filesystem::exists()` 函数检查同名 JSON 文件是否存在，如果 JSON 文件不存在，则先调用库文件解析功能，生成 JSON 文件，然后再执行单调性检查。
- 实现了`LibFile::mono()`方法，检查cell_rise、cell_fall、rise_transition、fall_transition这四种timing信息，针对其 LUT 数据结构，检查其 value 值在表格的每一行是否保持单调递增。

### 2025-02-20

- `LibFile::mono()`方法增加结果统计功能，在程序运行的最后输出单调性检查中 `passed` (通过) 和 `failed` (失败) 的 cell 数量，输出格式参考 `liberate_lv` 工具的风格。
- 以 `sl018_ff_3.96_-40.lib` 为例，与 `liberate_lv` 工具对比测试，结果均为 205 out of 559 cells failed。
- 对于非单调信息警告，增加输出 when 信息，方便用户查看具体位置。

### 2025-02-25

- 重构命令行参数解析，使用 `CLI11` 库的子命令，在每个回调函数中实现 `parse`、`mono` 功能。

### 2025-02-26

- 子命令可自定义输出文件名、日志文件名，并设置了相应的默认值。
- 改为在 Logger 初始化时打印版本信息，避免在 `-h`，`-v` 时多余打印。
- `LibFile::mono()` 方法增加对 `input_slew` 的单调性检查，如果在输入时有指定，就检查 value 矩阵的每一列是否单调。经过测试，`tcbn65lpbc.lib` 输出与 `liberate_lv` 工具一致。

### 2025-02-27

- 多线程并行尝试，采用GDB调试：
  - si2dr_liberty 库在解析 Liberty 文件时，可能使用了共享的数据结构（例如哈希表、字符串表）来存储解析结果或中间数据。
  - 在多线程并行解析多个文件时，不同的线程同时调用 si2dr_liberty 库的函数，并发地访问和操作这些共享数据结构。
  - si2dr_liberty 库可能没有采取足够的线程同步措施来保护这些共享数据结构，导致了数据竞争。
  - 数据竞争最终导致了内存损坏，使得 strcmp 函数在后续操作中访问了无效内存，触发了段错误。

### 2025-02-28

- 修改`LibFile`的构造函数与析构函数，`read`方法改为私有，移入parse方法。将si2dr初始化与销毁放在`parse`方法中，使得`mono`方法与si2dr库解耦，从而实现多线程并行验证单调性。
- [x] 顶层`si2drPIGetGroups`未退出的bug: `WARNING: si2drPIQuit: GetGroups called 1 more times than IterQuit`，内存泄漏？
- 学到了可以在`CMakeLists.txt`中使用`set(CMAKE_BUILD_TYPE Release)`减少编译器优化程度，配合GDB等调试工具检查堆栈信息。

### 2025-03-01

- `sub_groups_iter`是在每次`for`循环的迭代中声明的局部变量，在每次迭代结束后其作用域结束，析构函数被调用，释放相关资源。所以`si2drPIQuit`未发现内存泄漏。
- 顶层`group_iter`的析构函数发生在`parse`方法结束时，所以`parse`方法末尾的`si2drPIQuit`检测到了顶层groups未退出的情况。
- 综上，`si2drPIQuit`的警告是由于当时顶层groups未退出导致的，在`parse`方法结束后能正确释放，不会造成实际上的内存泄漏。
- [ ] 顺序执行`parse`，log输出仍存在问题：解析第二第三个库的时候会多余打印之前库的PVT信息，时间上也比单独解析一个库的总和要慢(parse 6s 左右，mono 1s)。并行执行`mono`，log输出会集中到最后一个库的文件。
- 给`LibFile`类的私有属性添加了初始值，避免了未初始化的问题，吗？

### 2025-03-07

- 实现了 `compare` 子命令的解析，能够对两个库文件依次进行解析，并生成对应的 JSON 文件。
- [ ] 仍然存在日志重复输出的问题，多文件处理时数据可能会相互干扰。

### 2025-03-10

- 新增 `supercell` 子命令和 `LibFile::supercell` 方法，用于生成超级单元以供验证。
- 集成了 `zlibboost` 子命令，支持调用开源库特征化工具 [zlibboost](https://github.com/skycrapers/ZlibBoost)。已与开发者取得联系。
- 调整了子命令输出文件的位置为当前目录，避免文件混淆。
- 添加了 `clear` 子命令，方便清理生成的 JSON 文件和日志文件。

### 2025-03-14

- 添加了 `tabulate/table` 库，通过 CMake FetchContent 进行管理，用于生成格式化的表格输出。
- 新建了 `compare.cpp` 和 `compare.hpp` 文件，创建了 `LibraryComparator` 类，用于比较两个库文件的差异。目前实现了读取参考库 JSON 文件的功能。

### 2025-03-15

- 使用 `std::filesystem::path` 重构了 `LibFile` 类的文件路径管理，方便文件路径的拼接和处理。`LibFile` 类添加了成员变量 `basename_`、`filename_`、`libname_`、`jsonname_` 和 `loggername_`，用于存储文件名、库名、JSON 文件名和日志文件名。
- 通过作用域 (scope) 管理 `LibFile::parse` 方法中的顶层迭代器的生命周期，提前结束并调用 `si2drPIQuit` 保证资源释放，解决了分析多个库时日志重复输出和数据保存错误的问题。
- `clear` 子命令增加了删除 `.map` 和 `.md` 文件的功能。
- 实现了 `mono` 子命令的多线程并行。首先检查是否是多文件输入，如果是，就顺序检查是否准备好了每个文件的 JSON 文件，否则顺序进行多文件库解析。在所有文件的 JSON 准备就绪后，根据文件数量创建线程池，每个线程负责一个库的单调性检查。
- 重构了 `spdlog` 日志初始化，返回 logger 对象而不是设置全局默认 logger。保留名为 `APP_NAME` 的全局 logger，用于输出总体提示信息。
- 为 `LibFile` 类添加了独立的成员变量 logger，用于记录每个库文件的日志信息。
- 完成了 `supercell` 子命令的多线程并行，处理逻辑与 `mono` 子命令类似。
- `LibraryComparator::generateReport()` 方法测试了 `tabulate` 库的markdown表格输出功能，完善了报告的头部信息输出，包括参考库、比较库、相对容差、软件信息、报告生成时间和图例说明。

### 2025-03-16

- 针对 `compare` 子命令，增加了对报告文件名的检查，确保其以 `.md` 或 `.txt` 结尾。若不符合，则给出警告并自动添加 `.md` 后缀。
- 进一步完善了 `LibraryComparator::generateReport()` 方法。现在，该方法能够遍历比较库中的所有 cell，并在参考库中查找对应的 cell。如果找到，则调用 `LibraryComparator::compareCell()` 方法进行比较；否则，会记录 cell 未找到的警告信息。
- 新增了 `LibraryComparator::compareCell()` 方法，用于比较两个库中同名 cell 的输出引脚。该方法会遍历比较库 cell 的每个输出引脚，在参考库 cell 中寻找同名引脚。如果找到，则调用 `comparePin` 函数比较引脚；如果未找到，则记录警告信息。如果比较库 cell 没有输出引脚，则会记录一条信息级别的日志。
- 新增了 `LibraryComparator::comparePin()` 方法，用于比较两个库中同名 cell 的同名引脚。它会遍历比较库 pin 的每个 timing arc，并在参考库 pin 中查找相同类型的 timing arc。如果找到，则调用 `compareTimingArc` 函数进行比较；如果未找到，则记录警告信息。如果比较库 pin 没有 timing arc，则记录一条信息级别的日志。
- 修改了 `LibraryComparator::compareTimingArc()` 方法，用于比较两个 timing arc JSON 对象。该方法首先记录正在比较的 timing arc 的类型，然后提取 "related_pin" 属性。接着，它遍历预定义的 timing arc 名称列表（"cell_rise", "cell_fall", "rise_transition", "fall_transition"），并在比较库中查找这些 timing arc。如果找到，则在参考库中查找对应的 timing arc，并调用 `compareLut` 方法进行比较。如果参考库中未找到对应的 timing arc，则记录警告信息。
- 修改了 `LibraryComparator::compareLut()` 方法（原 `compareValue` 方法，已更正命名），用于比较两个 JSON 对象中具有相同名称的 value。它首先提取两个 JSON 对象中的 "index_1" 和 "index_2" 数组，如果这两个数组不相等，则记录错误信息并返回。如果索引匹配，则比较 LUT 中的实际 value 值，并根据相对容差 (`reltol`) 和绝对容差 (`abstol`) 标记差异。
- 重新设计了层级比较方法，现在可以逐层级地传入 `cell_name`、`pin_name`、`timing_type`、`related_pin` 和 `arc_name`，以便在最内层制表或提示时能够准确输出层级信息。
- 修复了 `LibraryComparator::compareValue()` 方法的命名错误，已更正为 `LibraryComparator::compareLut()`。
- 已经可以输出表格，数据数量和商用工具结果一致，但格式还需要进一步调整。
- 新增了 `abstol` 参数，默认值为 0.002ns，用于设置绝对容差，与库验证工具保持一致。

### 2025-03-17

- 新增 `verilog` 和 `spice` 子命令，用于生成 Verilog 和 SPICE 代码。
- 完善 `LibraryComparator` 报告生成：
  - 为每个 cell 增加表头，仅在存在表格数据时输出表格，避免冗余输出。
  - 增加 cell 结果统计功能，输出结果统计表格，目前仅支持 Timing 中 Delay 的比较。

### 2025-03-18

- 优化统计表格，包含 | Cell Name | Data Type | Failed Count | Avg Diff | Avg Diff% | Max Diff | Max Diff% | Outliers |。
- 优化报告对比表格，包含 | Pin Name | Reference | Comparison | Diff | Diff % | Type | Arc Name | Row # | Index_1 | Column # | Index_2 | Note |
- 报告表格的 stdout 输出增加表头加粗、黄色的格式化。
- 增加 `si2drSimpleAttrGetBooleanValue` 封装，用于获取布尔类型的属性值，以判断引脚是否是时钟引脚。
- `LibFile::mono()` 增加对 `min_pulse_width` 的单调性检查：
  - 针对包含 "input_pins" 的 cell，遍历每个输入引脚。
  - 如果引脚是时钟引脚，则检查其 "timing_arcs"。
  - 针对 "timing_type" 为 "min_pulse_width" 的 timing arc，对 "rise_constraint" 和 "fall_constraint" 调用 `checkTimingArcMonotonicity` 进行单调性检查。
- `checkTimingArcMonotonicity` 函数日志区分有无 when 信息，输出不同日志。
- `checkTimingArcMonotonicity` 核心比较功能取消了等于的情况，相等情况默认为通过。
- `checkTimingArcMonotonicity` 核心比较功能增加了判断 `related_pin` 是否等于当前 pin，不等于则跳过。最终改为：如果矩阵中当前的值小于前一个值，并且当前引脚不是相关引脚，或者当前值和前一个值都为零，那么就认为这些值不是单调递增的。

### 2025-03-19

- 完善supercell方法，如果有有时钟信号引脚，链式长度被设为1，再生成超级单元。
- 解决 voltage 浮点数误差问题，std::round(voltage_ * 100) / 100.0 保留两位小数。

### 2025-03-21

- 新增 func 子命令，用于检查库或者Verilog文件的逻辑等价性。

### 2025-03-24

- 调研了 C++ 操作 Verilog 相关的库，找到了一个实用的 Verilog 库：[slang](https://github.com/MikePopoloski/slang)，它可以解析出抽象语法树 (AST)。已修改 CMakeLists 文件，将 slang 集成到项目中，目前正在研究其 API。

### 2025-03-25

- 新增 `verilog_utils.cpp` 和 `verilog_utils.hpp` 文件，并创建了 `VerilogVisitor` 类，用于自定义遍历 Verilog AST。
- 验证了 slang 对不同类型逻辑单元的解析能力：
  - 简单组合逻辑（如 `AND2D0`）、复杂时序逻辑（如 `CMPE42D1`）和基本时序逻辑（如 `DFQD1`）均能被正确解析。
  - 用户自定义原语 (UDP)，例如 `tsmc_dff`，会被错误地识别为模块实例化，并产生 "Invalid instance declaration" 警告。
  - 成功提取了模块名、端口方向（`input`、`output`）及名称。
  - 能够解析命名端口连接的层次化实例化，并获取模块名、实例名称和端口映射关系。
  - 能够解析门级原语实例化，并获取门类型、实例名称、输出端口和输入端口。

### 2025-03-26

- 进一步完善了对 UDP（如 `tsmc_dff`）端口映射关系的解析，但其内部 Entry 尚未处理。
- 尝试了通过继承 `slang::syntax::SyntaxRewriter` 类创建自定义类 `CellExtractor`，实现了只保留目标模块、删除其他模块，并将修改后的语法树另存为新文件。使用 `slang::syntax::SyntaxPrinter::printFile()` 方法可以将 Verilog 代码输出到文件。
- 创建了 `CellPrinter` 类，继承自 `slang::syntax::SyntaxVisitor` 类。当访问到目标模块时，使用 `module.toString()` 方法也能将 Verilog 代码输出到文件，但发现输出结果中空行会被删除。
- 修改了 `LibFile::supercell()` 方法，该方法可以根据输入的 `cell_names` 生成指定的 supercell，并在遇到未找到的单元时提示警告信息。
- 优化了时钟引脚的处理方式，现在时钟引脚不再被视为 supercell 的输入引脚，而是仅记录为时序单元，并跳过存入 `input_pins` 集合的步骤。
- 引入了 Doxygen 文档生成工具，用于可视化分析项目，并生成了 Doxygen HTML 文档和 LaTeX 参考手册。
- [x] 尚未解决手册中文显示不正确的问题，可能需要自定义 LaTeX 头文件。

### 2025-03-27

- **重大失误！** 误操作 Git 分支，导致部分代码修改丢失。务必牢记：**及时提交代码！**
- 实现了 `LibFile::verilog` 方法的核心流程：
  - 首先调用 `this->supercell()` 生成超级单元。
  - 读取 `.map` 文件，提取单元映射关系到 `std::pair<std::string, std::string>` 中（原始单元名 -> 超级单元名/模块名）。
  - 从 `lib_json_` 中提取单元的输入/输出端口信息。
  - 根据是否存在时钟引脚，确定 Verilog 模块中 instance 的生成数量（时序逻辑为 1，组合逻辑等于 chain length）。
  - 通过字符串操作生成 ANSI 风格的端口列表，并与模块名组合成完整的 `fullModuleText`。
  - 使用 `slang::syntax::SyntaxTree::fromText` 从 `fullModuleText` 生成语法树。
  - 将语法树传递给 `ModuleRewriter` 类，使用其 `transform` 方法遍历模块成员，并通过 `handle()` 方法进行处理。
  - 最后，使用 `slang::syntax::SyntaxPrinter::printFile(*tree)` 将处理后的语法树输出到文件。
- `LibFile::verilog` 目前能够正确输出模块名和 ANSI 风格的端口声明。

### 2025-03-28

- 实现了在模块成员列表中插入新节点的功能。具体而言，在 `ModuleRewriter::handle(const slang::syntax::ModuleDeclarationSyntax &module)` 方法中，通过 `insertAtBack(module.members, newDataNode)` 函数，成功地在模块成员的末尾插入了链式单元所需的中间变量声明，例如 `wire OP_i;`。
- 成功地在模块内部添加了关键连接网络变量。通过解析 `CELLNAME__X#__CRITICALPORT__OUTPUTPORT` 格式的字符串，提取出关键输入端口 (`critical input port`) 和输出端口 (`OUTPUTPORT`)，用于后续的实例化端口连接。
- 增加了用于处理多输出接口的中间网络变量 `P__UNUSEOUTPUTPORT`，用于连接不需要考察的输出端口。
- 成功地在模块内添加了模块实例化所需的模块名和实例名称，完成了端口映射关系的连接处理。
- 验证了代码对简单组合逻辑单元（如 `INVD0` 反相器）和复杂组合逻辑单元（如 `FA1D0` 全加器）的输出能力，结果均符合预期。
- 为 `ModuleRewriter` 类添加了私有属性 `std::map<std::string, std::string> portInfoMap_`，用于存储端口映射关系，将端口名映射到其方向（`input/output`）。
- 完成了实例化端口连接的细节处理：
  - 如果是关键输入端口且为第一个实例，则直接连接到输入端口；否则，连接到 `OP_(i-1)` 这样的中间网络变量。
  - 如果是关键输出端口，且为最后一个实例，则连接到输出端口；否则，连接到 `OP_i` 中间网络变量。
  - 如果是其他输出端口且不是最后一个实例，则连接到 `P_i__portName` 这样的中间网络变量；否则，连接名等于端口名。
  - 其余输入端口，连接名等于端口名。
- 文档方面，GitHub 远程仓库已开放为公开访问，并配置了 GitHub Pages。可以通过 [https://cedar17.github.io/ZlibValidation/](https://cedar17.github.io/ZlibValidation/) 访问项目文档，该页面包含 Doxygen 生成的 HTML 文档和仓库地址的链接。
- 配置了 GitHub Actions，实现了 Doxygen 文档和 Graphviz 图的自动化构建，以及 LaTeX 参考手册的自动编译（暂不支持中文）。每次在 `dev` 和 `main` 分支的提交都会触发文档和参考手册的生成，并发布到 `gh-pages` 分支。
- 将第三方库放置到 `include_3rd_party` 目录下，方便管理。修改了 CMakeLists 文件，将第三方库的头文件路径添加到 `include_directories` 中。修改了 Doxyfile 文件，将第三方库的头文件路径添加到 `INPUT` 中，便于文档生成工具理解第三方库函数、类的关系及使用方法。

### 2025-03-29

- 修复了 Doxygen 生成 LaTeX 参考手册时中文显示不正确的问题。通过修改 Doxyfile 文件，添加 `EXTRA_PACKAGES = ctex` 选项，并指定 `lualatex` 作为编译器，成功解决了中文显示问题。同时，从 `include_3rd_party` 目录中移除了 `Allsyntax.h` 文件，避免了由于 LaTeX 文档过大导致的 "TeX capacity exceeded, sorry [main memory size=5000000]" 内存不足问题。
- 实现了 Verilog 文件的顶层模块生成功能，采用非 ANSI 风格的端口声明方式，格式与 `liberate_lv` 工具生成的完全一致。实现过程如下：
  - 首先将各模块的 Verilog 代码存放到临时文件中。
  - 在处理模块的过程中，使用 `std::vector<std::string>` 收集每个模块的 `input_pins` 和 `output_pins`。
  - 遍历所有模块名称列表，拼接模块名和引脚名，生成顶层模块的 `all_input_ports` 和 `all_output_ports`。
  - 使用字符串拼接方式构建 `validate_top` 模块的完整代码。
  - 最后将临时文件内容与顶层模块代码合并，输出到最终的 Verilog 文件中。
- 更改`deploy-docs.yml`，修复了 GitHub Action 生成的 LaTeX 参考手册中的图像显示问题。具体步骤如下：
  - 设置系统时区为 Asia/Shanghai (东八区)。
  - 安装 miniconda 并更新 conda。
  - 通过 conda 安装 1.9.5 版本的 Doxygen，并安装 graphviz 以解决依赖关系，确保每个 dot 图都能正确编译出 pdf 文件。
  - 使用 `doxygen` 命令生成 HTML 文档和 tex 文件。
  - 采用 action marketplace 提供的 `xu-cheng/texlive-action@v2` 安装全量的 texlive2024。
  - 进入 `./doc_doxygen/latex` 目录，运行 `make` 命令使用 lualatex 编译 tex 文件，生成 pdf 文件。
  - 使用 `JamesIves/github-pages-deploy-action@v4` 将生成的 pdf 文件上传到 GitHub Pages 发布，并保留 `README.md` 和 `index.html` 文件。

### 2025-03-30

- 完成了 `LibFile::logic(const std::string &cell_name)` 方法，该方法返回指定 cell_name 所有输出引脚对应的逻辑表达式，以 `std::map<std::string, std::string>` 的形式返回，存储输出引脚名称到其逻辑表达式字符串的映射关系.
- 实现了 `func` 子命令的基本框架，用于检查库或 Verilog 文件的逻辑等价性。
  - 完善了 `funcLibFile` 函数，能够处理 Liberty 文件和 Verilog 文件作为参考和比较对象。
  - 增加了文件类型判断，根据文件扩展名选择不同的处理方式。
  - 实现了对 `.lib` 文件的解析，并提取指定 cell 的逻辑表达式。
  - 目前仅支持 Liberty 文件的逻辑表达式提取，Verilog 文件的逻辑表达式提取功能尚未完成（TODO）。
  - 增加了对 report 文件名的检查，确保其以 `.md` 或 `.txt` 结尾。若不符合，则给出警告并自动添加 `.md` 后缀。
  - 实现了从 Liberty 文件中提取逻辑表达式的功能，使用了 `LibFile::logic` 方法。
  - 增加了日志输出，记录每个 cell 的逻辑表达式提取和比较过程。
  - 逻辑比较部分目前为空，待实现（TODO）。

### 2025-03-31

- 实现了从 Verilog 结构网表中提取逻辑表达式的功能，使用了 `LogicExtractor` 类。
  - 设计了如何使用 Slang 库从 Verilog 结构网表中提取逻辑表达式的方案：
    - 使用 Slang 解析 Verilog 文件获取语法树(AST)。
    - 遍历 AST 定位目标模块。
    - 在目标模块内构建网表的内部表示，识别输入、输出、线网及门级连接关系。
    - 对每个输出端口，递归反向追踪门和线网直至到达主输入端口。
    - 在回溯过程中根据遇到的门类型构建逻辑表达式。
    - 应用记忆化技术避免对相同线网/信号的重复计算。
  - 创建了 `LogicExtractor` 类，用于从 Verilog 代码中提取逻辑表达式。
    - `LogicExtractor` 类使用 Slang 库解析 Verilog 文件，并构建目标模块的网表表示。
    - 实现了 `handle(const slang::syntax::ModuleDeclarationSyntax &module)` 方法，用于定位目标模块，并初始化内部数据结构。
    - 实现了 `handle(const slang::syntax::PortDeclarationSyntax &portDecl)` 方法，用于提取端口信息，包括端口方向和名称。
    - 实现了 `handle(const slang::syntax::NetDeclarationSyntax &netDecl)` 方法，用于提取线网信息。
    - 实现了 `handle(const slang::syntax::PrimitiveInstantiationSyntax &primitiveInst)` 方法，用于提取门级单元信息，包括门类型、输入和输出信号。
    - 实现了 `getLogicExpressions()` 方法，用于根据提取的网表信息，递归地推导每个输出端口的逻辑表达式。
    - 实现了 `deriveLogicRecursive(const std::string &signalName)` 方法，用于递归地推导指定信号的逻辑表达式。
    - 实现了 `formatExpression(const GateInfo &gateInfo, const std::vector<std::string> &inputExprs)` 方法，用于根据门类型和输入表达式，生成逻辑表达式字符串。
  - 实现了 `extractAndPrintNetlistInfo(const std::string &verilog_file, const std::string &cell)` 函数，用于提取并打印网表信息，包括输入、输出、线网和门级单元。
  - 实现了 `extractLogicFromVerilog(const std::string &verilog_file, const std::string &cell)` 函数，用于从 Verilog 文件中提取指定 cell 的逻辑表达式，并以 `std::map<std::string, std::string>` 的形式返回。
- 为了更好地组织代码结构，将 `LogicExtractor` 类从 `verilog_utils.hpp` 和 `verilog_utils.cpp` 中分离出来，并创建了独立的 `LogicExtractor.hpp` 和 `LogicExtractor.cpp` 文件。同时，统一了所有 hpp 和 cpp 文件的命名规范，使其与对应的类名保持一致。
- 新增 `LogicComparator` 类及其头文件、源文件，用于比较两个逻辑表达式的等价性，并生成比较报告。

### 2025-04-01

- 整理头文件引用，删除了不必要的引用，提高编译效率。
- 引入 `exprtk.cpp` 到第三方头文件目录，为后续的表达式解析做准备。
- 完善了 `LogicComparator` 类的实现，增加了其示例代码模板 `template <typename T> void logic();`，该模板能够对一个表达式遍历输入变量的所有组合，计算出逻辑值，并打印真值表，为逻辑等价性验证提供基础。
- 实现了 `LogicComparator::preprocessExpression()` 方法，用于预处理逻辑表达式，目的是简化表达式，使其更易于比较。预处理的逻辑如下：
  - **统一 AND 处理**：将 `*` 和空格隐式 AND 都转换为带空格的 `and`，保证 AND 运算符的显式性。
  - **显式符号处理**：
    - 在所有括号 `()` 周围添加空格，方便后续分词。
    - 将 `+` 替换为 `or`。
    - 将 `^` 替换为 `xor`。
    - 将 `!` (非 `!=` 中的 `!`) 替换为 `not`。
    - 将 `*` 替换为 `and`。
  - **分词**：基于空格将处理后的字符串分割为 token 列表。
  - **精确插入隐式 AND**：
    - 遍历 token 列表。
    - 检查当前 token 和下一个 token，如果满足以下条件，则在当前 token 之后插入 `and`：
      - 当前 token 是一个标识符 (如 `A`, `CIX`) 或右括号 `)`。
      - 下一个 token 是一个标识符或左括号 `(`。
      - 当前 token 不是一个显式逻辑运算符 (`and`, `or`, `xor`, `not`) 或左括号 `(`。
      - 下一个 token 不是一个显式逻辑运算符或右括号 `)`。
  - **重组与清理**：将处理后的 token 列表用单个空格连接起来，并进行最终的空格清理（去除多余空格，修剪首尾空格），得到最终的预处理表达式。
- 在 `CMakeLists.txt` 中，强制启用了 lld 链接器和 ccache，以提升链接和编译速度。
- 为了保持 `main.cpp` 的整洁，将 `LibFile()` 函数的相关操作剥离出来，并放入 `LibFileOperations.cpp` 文件中。
- 实现了 `LogicComparator::extractVariables()` 方法，用于从两个表达式中提取并验证变量名，确保它们一致。
  - 该方法使用正则表达式初步提取所有可能的标识符。
  - 对每个提取出的标识符，先用 `isIdentifier` 函数（检查大写开头等）进行验证。
  - 通过 `isIdentifier` 验证后，仍然将其转换为小写并与 `keywords` 集合比较，以过滤掉可能的关键词（这是一个保守但安全的操作）。
  - 只有通过这两步验证的标识符才被认为是有效变量，并以原始大小写形式添加到各自的 `std::set` 中。
  - 最后比较两个 `std::set` 是否相等，如果不等则报告差异并返回 `false`，如果相等则将变量列表（保留原始大小写）排序后存入 `sorted_vars` 并返回 `true`。

- 修复了 `LogicComparator::preprocessExpression()` 方法中对 not 操作符的处理。exprtk 库要求 not 关键字后必须跟着括号，如 `not(A)` 而非 `not A`。为此增加了专门的处理步骤：
  - 遍历预处理后的 token 列表，识别所有 `not` 关键字。
  - 当发现 `not` 后，检查其后是否跟随标识符（通过 `isIdentifier()` 函数判断）。
  - 如果后接标识符，自动将 `not A` 形式转换为 `not(A)`，具体做法是依次添加 `not`、`(`、标识符和 `)`，并跳过已处理的标识符。
  - 如果后接其他元素（如 `(`、其他运算符或表达式结尾），保留原样，让 ExprTk 处理 `not(表达式)` 形式。
  - 对其他 token，直接添加到最终 token 列表，不做特殊处理。

- 实现了 `LogicComparator::compareSingleExpressionPair()` 方法，用于比较两个逻辑表达式的等价性：
  - 接收预处理后的表达式字符串和已排序的变量列表作为输入参数。
  - 为参考表达式和比较表达式分别创建 ExprTk 环境（符号表、表达式对象和解析器）。
  - 将所有变量添加到两个符号表中，确保变量一致性和顺序性。
  - 使用 ExprTk 解析器编译两个表达式，并检查可能的语法错误。
  - 通过二进制计数法，遍历所有可能的输入变量组合（2^N 种），其中 N 为变量数量。
  - 对每种组合：
    - 设置两个符号表中的变量值（0或1）。
    - 计算两个表达式的值，并处理可能的运行时错误。
    - 将计算结果转换为布尔值并存入结果向量。
    - 同步构建真值表，记录输入组合和对应的输出值。
  - 比较两个结果向量判断表达式是否等价。
  - 将生成的真值表存入 `PinComparisonResult` 对象的 `std::optional` 成员中，以便后续报告生成。

- 实现了`template <typename T> std::map<std::string, PinComparisonResult> LogicComparator::compareCellLogic();`
  - 该方法用于比较两个 cell 的逻辑表达式，返回一个 `std::map`，键为 cell 名称，值为 `PinComparisonResult` 结构体。
  - 遍历在类私有属性里的整个独立输出引脚键，提取其值作为逻辑字符串表达式，首先使用 `preprocessExpression` 进行预处理，然后使用 `extractVariables` 提取变量向量。
  - 调用 `LogicComparator::compareSingleExpressionPair()` 方法进行比较，结果存入 `PinComparisonResult` 对象。
  - 最后将 `PinComparisonResult` 对象存入 `std::map` 中，键为 cell 名称，值为 `PinComparisonResult` 对象，返回。

- 实现了 `LogicComparator::generateReport()` 方法，报告格式仍需要调整。

### 2025-04-02

- 取消所有模板函数的 `template <typename T>` 声明，改为使用 `double` 作为参数类型，加快编译速度。
- 完善了 `LogicComparator::generateReport()` 方法，增加了对逻辑比较结果的详细报告输出，包括：
  - 元数据，例如：**Performed by ZlibValidation v0.1.0 from Song Zixuan. on: Wed Apr  2 11:11:39 2025**
  - 图例，包括参考pin名称、pin function，参考pin名称、pin function，符号解释表。
  - 参考pin的真值表
  - 比较pin的真值表
  - 比较结果表格，例如：
  
| Property                 | Value                             |
| :----                    | :----                             |
| Status                   | [OK]                              |
| Reference (Raw)          | `(!((!(A1+A2))+B))`               |
| Comparison (Raw)         | `!((!A1 * !A2) + B)`              |
| Reference (Processed)    | `(not((not(A1 or A2)) or B))`     |
| Comparison (Processed)   | `not((not(A1) and not(A2)) or B)` |
| Ref Expression Compiled  | Yes                               |
| Comp Expression Compiled | Yes                               |
| Logically Equivalent     | Yes                               |
