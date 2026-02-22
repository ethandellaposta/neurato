cmake_minimum_required(VERSION 3.22)

# Find required packages
find_package(GTest REQUIRED)
find_package(GMock REQUIRED)

# Test executable
add_executable(neurato_tests
    TestSuite.cpp
    UnitTests.cpp
)

# Include directories
target_include_directories(neurato_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(neurato_tests PRIVATE
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    neurato_core
    neurato_engine
    neurato_ai
    neurato_model
    neurato_ui
    neurato_util
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
)

# Enable testing
enable_testing()

# Add test cases
add_test(NAME UnitTests COMMAND neurato_tests)

# Performance tests
add_test(NAME PerformanceTests COMMAND neurato_tests --gtest_filter="*Performance*")
set_tests_properties(PerformanceTests PROPERTIES TIMEOUT 300)

# Stress tests  
add_test(NAME StressTests COMMAND neurato_tests --gtest_filter="*Stress*")
set_tests_properties(StressTests PROPERTIES TIMEOUT 600)

# Integration tests
add_test(NAME IntegrationTests COMMAND neurato_tests --gtest_filter="*Integration*")
set_tests_properties(IntegrationTests PROPERTIES TIMEOUT 120)

# AI tests (may require model files)
add_test(NAME AITests COMMAND neurato_tests --gtest_filter="*AI*")
set_tests_properties(AITests PROPERTIES TIMEOUT 180)

# Set up test data
add_custom_target(test_data
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test_data
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tests/generate_test_data.py ${CMAKE_BINARY_DIR}/test_data
    COMMENT "Generating test data"
)

# Make tests depend on test data
add_dependencies(neurato_tests test_data)

# Code coverage (if available)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(neurato_tests PRIVATE --coverage)
    target_link_options(neurato_tests PRIVATE --coverage)
    
    add_custom_target(coverage
        COMMAND lcov --capture --directory ${CMAKE_BINARY_DIR} --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND lcov --list coverage.info
        COMMAND genhtml coverage.info --output-directory ${CMAKE_BINARY_DIR}/coverage
        COMMENT "Generating code coverage report"
    )
endif()

# Memory checking (if valgrind available)
find_program(VALGRIND_EXECUTABLE valgrind)
if(VALGRIND_EXECUTABLE)
    add_test(NAME MemoryTests COMMAND ${VALGRIND_EXECUTABLE} --leak-check=full --error-exitcode=1 neurato_tests)
    set_tests_properties(MemoryTests PROPERTIES TIMEOUT 600)
endif()

# Benchmark tests
add_executable(neurato_benchmarks
    TestSuite.cpp
    UnitTests.cpp
)

target_compile_definitions(neurato_benchmarks PRIVATE
    BENCHMARK_MODE=1
)

target_link_libraries(neurato_benchmarks PRIVATE
    ${CMAKE_SOURCE_DIR}/external/benchmark/libbenchmark.a
    neurato_core
    neurato_engine
    neurato_ai
    neurato_model
    neurato_ui
    neurato_util
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
)

add_custom_target(benchmarks
    COMMAND neurato_benchmarks --benchmark_format=json --benchmark_out=${CMAKE_BINARY_DIR}/benchmark_results.json
    COMMENT "Running performance benchmarks"
)

# Fuzz testing (if available)
option(ENABLE_FUZZING "Enable fuzz testing" OFF)
if(ENABLE_FUZZING AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(neurato_fuzzer
        fuzz/audio_graph_fuzzer.cpp
        fuzz/automation_fuzzer.cpp
        fuzz/plugin_host_fuzzer.cpp
    )
    
    target_compile_options(neurato_fuzzer PRIVATE
        -fsanitize=fuzzer,address
        -g
    )
    
    target_link_options(neurato_fuzzer PRIVATE
        -fsanitize=fuzzer,address
    )
    
    target_link_libraries(neurato_fuzzer PRIVATE
        neurato_core
        neurato_engine
        neurato_ai
        neurato_model
        neurato_ui
        neurato_util
    )
    
    add_custom_target(fuzz
        COMMAND neurato_fuzzer -max_len=4096 -dict=fuzz/dictionary.txt
        COMMENT "Running fuzz tests"
    )
endif()

# Continuous integration support
if(CI_BUILD)
    # Set strict test failure policies
    set_tests_properties(UnitTests PROPERTIES PASS_REGULAR_EXPRESSION ".*")
    set_tests_properties(IntegrationTests PROPERTIES PASS_REGULAR_EXPRESSION ".*")
    
    # Generate test reports
    add_custom_target(test_report
        COMMAND neurato_tests --gtest_output=xml:${CMAKE_BINARY_DIR}/test_results.xml
        COMMENT "Generating test report"
    )
endif()
