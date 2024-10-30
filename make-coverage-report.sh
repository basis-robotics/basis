#!/bin/bash
COVERAGE_DIR="coverage_reports"
HTML_INDEX_FILE=$COVERAGE_DIR/index.html

skip_tests=false
aggregate_report=false

# Parse command-line arguments
for arg in "$@"; do
    if [ "$arg" == "--skip-tests" ]; then
        skip_tests=true
    fi

    if [ "$arg" == "--aggregate" ]; then
        aggregate_report=true
    fi
done

# Create the coverage reports directory
mkdir -p "$COVERAGE_DIR"
rm -fr $COVERAGE_DIR/*

# Part you want to conditionally skip
if [ "$skip_tests" = false ]; then
    echo "Cleaning previous data..."
    find . -type f -name "*.profdata" -delete
    find . -type f -name "*.profraw" -delete

    echo "Running tests..."
    ctest
else
    echo "Skipping tests."
fi

# Find all test executables, excluding .py files and _deps directory
TEST_EXECUTABLES=$(find . -type f -executable -name "test_*" ! -name "*.yaml" ! -path "*/_deps/*")

if [ "$aggregate_report" = true ]; then
  echo "Generating aggregate coverage report"

  TEST_PROFDATA="coverage.profdata"
  TEST_PROF_RAWS=$(find . -name "*.profraw" ! -path "*/_deps/*")
  llvm-profdata-18 merge -sparse $TEST_PROF_RAWS -o $TEST_PROFDATA

  FILE_ARRAY=($TEST_EXECUTABLES)

  CMD="llvm-cov-18 show \
    --format=html \
    --instr-profile=\"$TEST_PROFDATA\" \
    -o \"$COVERAGE_DIR\" \
    -ignore-filename-regex='.*\/_deps\/.*' \
    -ignore-filename-regex='.*\/third_party\/.*'"
    # -ignore-filename-regex='.*\/test_.*.cpp' \

  CMD="$CMD ${FILE_ARRAY[0]}"

  # Loop over the remaining files and append them with the -o prefix
  for ((i=1; i<${#FILE_ARRAY[@]}; i++)); do
    CMD="$CMD -object ${FILE_ARRAY[i]}"
  done

  echo -e "\n\n\n"

  # Execute the command
  echo $CMD
  eval $CMD

else

  # Create an index.html file to link to each test's coverage report
  echo "<html><body><h1>Code Coverage Reports</h1>" > $HTML_INDEX_FILE

  # Iterate over each test executable
  for TEST_EXECUTABLE in $TEST_EXECUTABLES; do
    # Get the base name of the test executable
    TEST_NAME=$(basename "$TEST_EXECUTABLE")

    echo "Making folder for coverage report for $TEST_NAME"
    # Create a directory for this test's coverage report
    TEST_COVERAGE_DIR="$COVERAGE_DIR/$TEST_NAME"
    mkdir -p "$TEST_COVERAGE_DIR"
    
    EXECUTABLE_PROFRAW=$(realpath "$(dirname "$TEST_EXECUTABLE")/default.profraw")
    EXECUTABLE_PROFDATA=$(realpath "$(dirname "$TEST_EXECUTABLE")/default.profdata")

    EXECUTABLE-profdata-18 merge -sparse $TEST_PROFRAW -o $EXECUTABLE_PROFDATA

    echo "Generating coverage report for $TEST_NAME ($TEST_EXECUTABLE)"
    llvm-cov-18 show \
      --format=html \
      --instr-profile="$EXECUTABLE_PROFDATA" \
      -o "$TEST_COVERAGE_DIR" \
      -ignore-filename-regex='.*\/_deps\/.*' \
      -ignore-filename-regex='.*\/third_party\/.*' \
      "$TEST_EXECUTABLE"

    echo "<a href=\"$TEST_NAME/index.html\">$TEST_NAME</a><br>" >> $HTML_INDEX_FILE

    echo "Generated coverage report for $TEST_NAME in $TEST_COVERAGE_DIR"
    echo -e "\n"
  done

  echo "</body></html>" >> $HTML_INDEX_FILE
fi