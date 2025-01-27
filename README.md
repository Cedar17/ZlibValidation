# ZlibValidation

## Description

command line tool to validate standard cell libraries in `.lib` format.

## Usage

```bash
ZlibValidation
Usage: ./zlibvalidation [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -v,--version                Show version information
  --file TEXT:FILE            Specify the file to process
```

## Development Diary

### 2025-01-27

- Create blank project, use external `libsi2dr_liberty.a` library
- Add command line argument parsing using `CLI11` library
- Add `spdlog` logging library