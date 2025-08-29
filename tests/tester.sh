#!/usr/bin/env bash
set -e pipefail

BIN=bin/clox
INPUT_DIR=tests/inputs
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

for input_file in "$INPUT_DIR"/*;
do
    filename=$(basename -- "$input_file")
    expected_out="$EXPECTED_DIR/${filename%.in}.out.expected"
    expected_err="$EXPECTED_DIR/${filename%.in}.err.expected"
    expected_code="$EXPECTED_DIR/${filename%.in}.exitcode.expected"
    
    output_out="$OUTPUT_DIR/${filename%.in}.out"
    output_err="$OUTPUT_DIR/${filename%.in}.err"
    output_code="$OUTPUT_DIR/${filename%.in}.exitcode"

    echo ""
    echo "${BOLD}${YELLOW}Test: $filename${RESET}"
    if "$BIN" < "$input_file" > "$output_out" 2> "$output_err"; then
        echo "$?" > "$output_code"
    else
        echo "$?" > "$output_code"
    fi
    
    for f in "$expected_out" "$output_out" "$expected_err" "$output_err" "$expected_code" "$expected_code"; do
        [ -f "$f" ] && sed -i 's/\r$//' "$f"
    done

    ok=true
    
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

    # Compare stderr
    if [ -f "$expected_err" ]; then
        if ! diff -q "$expected_err" "$output_err" >/dev/null; then
            echo "[${RED}FAIL${RESET}] Stderr mismatch for $filename"
            diff --new-line-format="OUTPUT: ${RED}%L${RESET}" \
                 --old-line-format="EXPECTED: ${GREEN}%L${RESET}" \
                 --unchanged-line-format=" ${GREEN}%L${RESET}" \
                 "$expected_err" "$output_err" || true
            ok=false
        fi
    fi

    # Compare exit code
    if [ -f "$expected_code" ]; then
        if ! diff -q "$expected_code" "$output_code" >/dev/null; then
            echo "[${RED}FAIL${RESET}] Exit code mismatch for $filename"
            echo "EXPECTED: ${GREEN}$(cat "$expected_code")${RESET}"
            echo "OUTPUT:   ${RED}$(cat "$output_code")${RESET}"
            ok=false
        fi
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