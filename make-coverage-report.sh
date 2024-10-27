#!/bin/bash

# Directory to store all coverage reports
COVERAGE_DIR="coverage_reports"
HTML_INDEX_FILE=$COVERAGE_DIR/index.html

# Create the coverage reports directory
mkdir -p "$COVERAGE_DIR"
rm -fr $COVERAGE_DIR/*
rm *.profraw
rm *.profdata

COVERAGE_DATA_DIR=coverage_data
rm -fr $COVERAGE_DATA_DIR
mkdir -p $COVERAGE_DATA_DIR

# Find all test executables, excluding .py files and _deps directory
TEST_EXECUTABLES=$(find . -type f -executable -name "test_*" ! -name "*.yaml" ! -path "*/_deps/*")

# Create an index.html file to link to each test's coverage report
echo "<html><body><h1>Code Coverage Reports</h1>" > $HTML_INDEX_FILE

# Iterate over each test executable
for TEST_EXECUTABLE in $TEST_EXECUTABLES; do
  # Get the base name of the test executable
  TEST_NAME=$(basename "$TEST_EXECUTABLE")
  TEST_PROFRAW="$COVERAGE_DATA_DIR/$TEST_NAME.profraw"
  TEST_PROFDATA="$COVERAGE_DATA_DIR/$TEST_NAME.profdata"

  # Run the test executable
  echo running test executable: $TEST_EXECUTABLE
#   LLVM_PROFILE_FILE="$TEST_PROFRAW" $TEST_EXECUTABLE --gtest_brief=1
  export LLVM_PROFILE_FILE="$TEST_PROFRAW"
  "$TEST_EXECUTABLE" --gtest_brief=1
  # ctest -R "$TEST_EXECUTABLE" 

  echo "Merging coverage for $TEST_NAME"
  llvm-profdata-18 merge -sparse $TEST_PROFRAW -o $TEST_PROFDATA

  echo "Making folder for coverage report for $TEST_NAME"
  # Create a directory for this test's coverage report
  TEST_COVERAGE_DIR="$COVERAGE_DIR/$TEST_NAME"
  mkdir -p "$TEST_COVERAGE_DIR"
  
  echo "Generating coverage report for $TEST_NAME"
  # Generate the coverage report
  llvm-cov-18 show \
    --format=html \
    --instr-profile="$TEST_PROFDATA" \
    -o "$TEST_COVERAGE_DIR" \
    -ignore-filename-regex='.*\/_deps\/.*' \
    -ignore-filename-regex='.*\/third_party\/.*' \
    "$TEST_EXECUTABLE"
  
  echo "<a href=\"$TEST_NAME/index.html\">$TEST_NAME</a><br>" >> $HTML_INDEX_FILE

  echo "Generated coverage report for $TEST_NAME in $TEST_COVERAGE_DIR"
  echo -e "\n"
done

echo "</body></html>" >> $HTML_INDEX_FILE