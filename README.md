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

- 创建空项目，手动添加使用外部 `libsi2dr_liberty.a` 库
- 添加 `CLI11` 库处理命令行参数解析
- 添加 `spdlog` 日志库，高效日志格式化输出

### 2025-01-28

- 添加 `nlohmann/json` 库用于 JSON 数据存储和检索
- 在 `version.h` 中存储项目作者、版本、构建日期等信息，由 `CMake` 自动维护
- 简化了 `--version` 参数解析的代码实现，并添加了 `--mode` 选项以选择工作模式
- 使用 `spdlog` 库的日志记录器将 debug 级别及以上的日志输出到文件，将 info 级别及以上的日志输出到控制台
- 实现了 `LibFile` 类，用于封装读取、解析和修改库文件的功能，包含名称和 PVT 信息作为公共属性
  - 但仍然使用过程化方法读取和输出库文件名称和 PVT 信息
- 按照 C++ 标准库、第三方库和本地库的顺序重新排列了 `#include` 语句
