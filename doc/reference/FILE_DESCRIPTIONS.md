# JSON Database File Descriptions

This document provides descriptions for key files in the JSON Database project, focusing on their purpose and importance to the system.

## Core JavaScript Integration Files

### Engine Implementation

#### `/src/components/js/js_engine.c`
The heart of the JavaScript integration, implementing the JavaScript engine using QuickJS. This file bridges the gap between the native C database and JavaScript, providing bidirectional conversion between JSON and JavaScript data types. It includes conditional compilation support to provide either full JavaScript functionality or stub implementations based on the availability of QuickJS.

#### `/src/include/js/js_engine.h`
Header file defining the JavaScript engine interface. Declares the core functions for JavaScript execution and defines the `js_engine_t` structure that maintains the JavaScript runtime context. Uses conditional compilation to adapt the structure based on QuickJS availability.

### File Utilities

#### `/src/components/utils/js_file_utils.c`
Implements utilities for JavaScript file handling, including sophisticated path resolution to locate JavaScript files across multiple directories. Features a caching system for improved performance when resolving file paths repeatedly. The file search algorithm checks multiple common locations and applies necessary transformations to file paths.

#### `/src/include/utils/js_file_utils.h`
Header file for JavaScript file utilities, defining functions for finding JavaScript files, logging file search errors, setting cache paths, and saving cache data. Sets up the interface for the file resolution system that's critical for script loading.

## JavaScript Libraries

### `/functions/db_helpers.js`
Standard JavaScript helper library providing an elegant, object-oriented interface to the database. Implements two key classes:
1. `JsonDB` - Static class with methods for database operations
2. `Collection` - Object-oriented class for working with specific collections

This library significantly improves developer experience by providing a cleaner, more intuitive API than the raw database functions.

### `/functions/db_example.js`
Example JavaScript file demonstrating how to use the db_helpers.js library. Showcases common database operations including inserting documents, finding all documents in a collection, retrieving specific documents, updating documents, querying based on criteria, and deleting documents. Serves as both a functional test and developer documentation.

## Test Files

### `/tests/test_js_integration.c`
Comprehensive integration test for the JavaScript functionality. Tests basic JavaScript evaluation, database operations through JavaScript, and file evaluation. Provides a structured test suite that verifies the correct operation of all aspects of the JavaScript integration.

### `/tests/js_performance_benchmark.js`
JavaScript benchmark for evaluating database performance. Measures various aspects of the system including document insertion speed, query performance, update operations, delete operations, and complex operations. Provides essential metrics for understanding system performance characteristics.

### `/minimal_js_test.c`
Minimal test focusing solely on verifying QuickJS integration works correctly. Initializes the JavaScript runtime, runs a simple JavaScript expression, and verifies the result. Useful for basic sanity checks of the JavaScript engine without database dependencies.

### `/db_js_test.c`
Test program for evaluating JavaScript interaction with the database. Tests both basic JavaScript evaluation and database object access. Verifies that JavaScript code can properly access and use the database API.

### `/tests/generate_performance_report.js`
Advanced performance testing script that generates comprehensive performance reports. Runs multiple iterations of tests with different data set sizes and operation types, calculating detailed statistics. Provides a structured JSON output suitable for visualization and analysis.

## Documentation Files

### `/docs/JAVASCRIPT_API.md`
Comprehensive documentation of the JavaScript API for the JSON database. Details both the direct database API (`db` object) and helper library. Includes examples, usage patterns, validators and transformers, user-defined functions, and performance considerations. Serves as the primary reference for JavaScript developers using the system.

### `/docs/IMPLEMENTATION_STATUS.md`
Overview of the current implementation status of the entire JSON database server. Covers core components, JavaScript integration, performance status, code quality, current limitations, recent improvements, and future development areas. Provides a high-level view of project status for stakeholders.

### `/docs/JAVASCRIPT_INTEGRATION_REVIEW.md`
Detailed review and analysis of the JavaScript integration. Evaluates the architecture, integration quality, API design, testing quality, performance, and code quality. Includes recommendations for future enhancements. Serves as both documentation and a roadmap for further development of the JavaScript integration.

### `/docs/JS_PATH_RESOLUTION.md`
Documentation of the JavaScript file path resolution system. Explains the search algorithm, caching mechanisms, and configuration options. Important for developers who need to understand how JavaScript files are located and loaded by the system.

## Build and Test Scripts

### `/run_performance_benchmark.sh`
Master script for running the complete performance benchmark suite. Orchestrates the execution of performance tests, generation of detailed reports, and creation of HTML visualizations. Provides a comprehensive view of system performance.

### `/tests/run_performance_tests.sh`
Script for running basic performance tests. Sets up a clean test environment, starts the database server, runs the JavaScript performance benchmark, collects statistics, and performs cleanup. Essential for regular performance testing.

### `/tests/generate_html_report.py`
Python script that converts JSON performance data into an interactive HTML report with visualizations. Takes the output from performance tests and creates a user-friendly report for analyzing results. Enhances the interpretability of performance test data.

### `/tests/performance_report_template.html`
HTML template for performance reports. Includes JavaScript for rendering charts and visualizations of performance data. Used by the generate_html_report.py script to create interactive performance reports.

## JSON Database Server Scripts

### `/tests/test_js_hello.sh`
Script for testing basic JavaScript functionality through the server. Runs a simple hello_world.js script using the database server and verifies the output. Used as a quick functional test of the JavaScript integration.

### `/tests/test_js_integration.sh`
Script for running the JavaScript integration tests. Ensures the database server is properly running, executes the test suite, and reports results. Key for verifying the overall health of the JavaScript integration.

### `/test_server_control.sh`
Script for controlling the database server during testing. Handles starting, stopping, and checking the status of the server. Essential for automated testing that requires a running server instance.

## Source Code Organization

### `/src/include/`
Directory containing all header files for the project. Organized into subdirectories based on component (js, database, utils, etc.). Defines the public interfaces for all components of the system.

### `/src/components/`
Directory containing the implementation files for all components. Mirrors the structure of the include directory. Contains the actual implementation of all the functionality declared in the include files.

### `/functions/`
Directory for JavaScript files that extend the database functionality. Contains helper libraries, validation functions, transformation functions, and examples. The main location for JavaScript code in the project.

### `/tests/`
Directory containing all test files. Includes unit tests, integration tests, performance tests, and test scripts. Crucial for maintaining code quality and verifying functionality.

### `/docs/`
Directory containing all documentation files. Organized into subdirectories for different types of documentation. Provides comprehensive information about all aspects of the system.

## Configuration and Example Files

### `/config.json.example`
Example configuration file showing the available options for the database server. Illustrates how to configure the server, including network settings, database paths, and performance options.

### `/js_test.js`
Simple JavaScript test file in the project root. Tests basic JavaScript functionality and interaction with the database. Useful for quick manual testing of the JavaScript engine.

## Summary

The JSON database project features a robust implementation with a particular emphasis on JavaScript integration. The file organization reflects a clean separation of concerns, with header files defining interfaces and implementation files providing the actual functionality. The JavaScript integration is implemented through a combination of C code for the engine and file utilities, along with JavaScript helper libraries for improved developer experience.

Testing is comprehensive, with dedicated test files for different aspects of the system and scripts for running various types of tests. Documentation is extensive, covering both technical details for developers and higher-level information for stakeholders.

The project demonstrates a strong focus on quality, with attention to performance, usability, and maintainability evident throughout the codebase.