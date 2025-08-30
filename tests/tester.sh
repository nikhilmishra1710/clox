#!/usr/bin/env bash
set -euo pipefail

BIN=bin/clox
INPUT_DIR=tests/specs
EXPECTED_DIR=tests/expected
OUTPUT_DIR=tests/outputs

echo "Setting up output directory..."
mkdir -p $OUTPUT_DIR

RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
YELLOW=$(tput setaf 3)
BLUE=$(tput setaf 4)
BOLD=$(tput bold)
RESET=$(tput sgr0)

pass=0
fail=0

for test_spec_file in "$INPUT_DIR"/*.yaml;
do
    filename=$(basename -- "$test_spec_file" .yaml)

    output_out="$OUTPUT_DIR/${filename}.out"
    output_err="$OUTPUT_DIR/${filename}.err"

    test_id=$( yq -r '.id' "$test_spec_file" )
    
    infile_type=$( yq -r '.input.type' "$test_spec_file" )

    if [ "$infile_type" == "file" ]; then
        infile=$( yq -r '.input.file' "$test_spec_file" )
        if [ ! -f "$infile" ]; then
            echo "[${RED}ERROR${RESET}] Input file $infile does not exist for test $test_id"
            fail=$((fail + 1))
            continue
        fi
        cp "$infile" "$OUTPUT_DIR/${filename}.in"
    elif [ "$infile_type" == "inline" ]; then
        infile_content=$( yq -r '.input.inline' "$test_spec_file" )
        echo "$infile_content" > "$OUTPUT_DIR/${filename%.in}.in"
        infile="$OUTPUT_DIR/${filename%.in}.in"
    else
        echo "[${RED}ERROR${RESET}] Unknown infile type for test $test_id"
        fail=$((fail + 1))
        continue
    fi

    echo "${BOLD}${YELLOW}Test ID: $test_id $test_spec_file${RESET}"

    status=0
    if "$BIN" < "$infile" > "$output_out" 2> "$output_err"; then
        status=$?
    else
        status=$?
    fi
    
    # for f in "$expected_out" "$output_out" "$expected_err" "$output_err" "$expected_code" "$expected_code"; do
    #     [ -f "$f" ] && sed -i 's/\r$//' "$f"
    # done

    ok=true
    
    # Compare stdout
    if [ $(yq -r '.expected_stdout.type' "$test_spec_file") == "file" ]; then
        expected_out=$( yq -r '.expected_stdout.file' "$test_spec_file" )
        if [ -f "$expected_out" ]; then
            if ! diff -q "$expected_out" "$output_out" >/dev/null; then
                ok=false
            fi
            diff --new-line-format="OUTPUT: ${RED}%L${RESET}" \
                --old-line-format="EXPECTED: ${GREEN}%L${RESET}" \
                --unchanged-line-format="OUTPUT: ${GREEN}%L${RESET}" \
                "$expected_out" "$output_out" || true
        else
            echo "[${YELLOW}WARN${RESET}] No expected stdout file for $filename"
            ok=false
        fi
    elif [ $(yq -r '.type' "$test_spec_file") == "inline" ]; then
        expected_out=$( yq -r '.expected_stdout.inline' "$test_spec_file" )
        if [ -n "$expected_out" ]; then
            if ! diff -q <(echo "$expected_out") "$output_out" >/dev/null; then
                ok=false
            fi
            diff --new-line-format="OUTPUT: ${RED}%L${RESET}" \
                --old-line-format="EXPECTED: ${GREEN}%L${RESET}" \
                --unchanged-line-format="OUTPUT: ${GREEN}%L${RESET}" \
                <(echo "$expected_out") "$output_out" || true
        else
            echo "[${YELLOW}WARN${RESET}] No expected stdout inline content for $filename"
            ok=false
        fi
    else
        echo "[${YELLOW}WARN${RESET}] Unknown expected_stdout type for $filename"
        ok=false
    fi

    # Compare stderr
    if [ $(yq -r '.expected_stderr.type' "$test_spec_file") == "file" ]; then
        expected_err=$( yq -r '.expected_stderr.file' "$test_spec_file" )
        if [ -f "$expected_err" ]; then
            if ! diff -q "$expected_err" "$output_err" >/dev/null; then
                ok=false
            fi
            diff --new-line-format="OUTPUT: ${RED}%L${RESET}" \
                --old-line-format="EXPECTED: ${GREEN}%L${RESET}" \
                --unchanged-line-format="OUTPUT: ${GREEN}%L${RESET}" \
                "$expected_err" "$output_err" || true
        else
            echo "[${YELLOW}WARN${RESET}] No expected stdout file for $filename"
            ok=false
        fi
    elif [ $(yq -r '.type' "$test_spec_file") == "inline" ]; then
        expected_err=$( yq -r '.expected_stderr.inline' "$test_spec_file" )
        if [ -n "$expected_err" ]; then
            if ! diff -q <(echo "$expected_err") "$output_err" >/dev/null; then
                ok=false
            fi
            diff --new-line-format="OUTPUT: ${RED}%L${RESET}" \
                --old-line-format="EXPECTED: ${GREEN}%L${RESET}" \
                --unchanged-line-format="OUTPUT: ${GREEN}%L${RESET}" \
                <(echo "$expected_err") "$output_err" || true
        else
            echo "[${YELLOW}WARN${RESET}] No expected stdout inline content for $filename"
            ok=false
        fi
    else
        echo "[${YELLOW}WARN${RESET}] Unknown expected_stderr type for $filename"
        ok=false
    fi

    # Compare exit code
    expected_code=$( yq -r '.expected_ec' "$test_spec_file" )
    if [ "$status" -ne "$expected_code" ]; then
        echo "Exit code mismatch"
        echo "EXPECTED: ${GREEN}$expected_code${RESET}"
        echo "OUTPUT: ${RED}$status${RESET}"
        ok=false
    else
        echo "Exit code: ${GREEN}$status${RESET}"
    fi

    if $ok; then
        echo "[${GREEN}PASS${RESET}] Test $filename: PASS"
        pass=$((pass + 1))
    else
        echo "[${RED}FAIL${RESET}] Test $filename: FAIL"
        fail=$((fail + 1))
    fi
done

echo "----------------------------------------"
echo "Total tests: $((pass + fail)), Passed: $pass, Failed: $fail"
echo "----------------------------------------"