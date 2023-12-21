# Memory Leak Resolution Tool

## Overview
This tool was developed as part of a Bachelor thesis. It is designed to streamline the process of fixing memory leaks in C++ source code. It targets educational contexts, aiding both educators and students. The tool extends beyond leak detection, offering suggestions for where memory should be deallocated in the source code.

## Features
- **Integration with Valgrind**: Utilizes Valgrind's memcheck analysis to identify memory leak locations.
- **Memory Ownership Model**: Employs a proposed model with heuristics for resolving various memory leak scenarios.
- **Abstract Syntax Tree Analysis**: Uses libclang to parse and analyze C++ source code for suggesting deallocation locations.

## Requirements
- C++ environment for running the tool.
- Valgrind installed for memory leak detection.
- libclang library for parsing source code.

## Usage
1. Compile your C++ source code with debug flags.
2. Run the tool with the path to your source file as an argument.
3. Review the suggestions provided by the tool for memory leak resolution.

## Limitations
- The tool is primarily tested in an educational setting with simple C++ programs.
- It currently does not support multithreaded applications.
- Relies on specific code formatting and Valgrind for leak detection.

## Future Work
- Expanding the types of memory leaks covered.
- Developing an independent leak detection mechanism.
- Enhancing the tool to accommodate different code styles and multithreading.


## Acknowledgements
 at Vrije Universiteit Amsterdam by Hung Hoang Duy, under the supervision of Thilo Kielmann and Felienne Hermans.
