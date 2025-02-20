# ZlibValidation

## Description

command line tool to validate standard cell libraries in `.lib` format.

## Usage

```zsh
ZlibValidation
Usage: ./zlibvalidation [OPTIONS]

Options:
  -h,--help                                  Print this help message and exit
  -v,--version                               Display program version information and exit
  -m,--mode TEXT:{parse,modify} [parse]      Choose the mode: 'parse' or 'modify'
  -f,--file TEXT:FILE REQUIRED               Specify the file to process
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
- [ ] 时序 (timing) 信息解析功能待完成。
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
- 对于非单调信息警告，增加输出 when 信息，方便用户查看具体位置。