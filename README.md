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
- [ ] voltage 浮点数误差未解决。

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