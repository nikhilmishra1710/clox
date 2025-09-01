#!/usr/bin/env bash
set -euo pipefail

BIN=bin/clox
INPUT_DIR=tests/specs
EXPECTED_DIR=tests/expected
OUTPUT_DIR=tests/outputs
TMP_DIR=tests/tmp

echo "Setting up output directory..."
mkdir -p $OUTPUT_DIR

mkdir -p $TMP_DIR

RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
YELLOW=$(tput setaf 3)
BLUE=$(tput setaf 4)
BOLD=$(tput bold)
RESET=$(tput sgr0)

pass=0
fail=0
declare -a results=()

run_hooks() {
    local type=$1
    local path=$2
    if [ "$type" == "file" ]; then
        hooks=$( yq -r "$path.file | if type==\"string\" then . else .[] end" "$test_spec_file" )
    elif [ "$type" == "inline" ]; then
        hooks=$( yq -r "$path.inline | if type==\"string\" then . else .[] end" "$test_spec_file" )
    else
        echo "[${YELLOW}WARN${RESET}] Unknown hooks type"
        return 1
    fi
    while IFS= read -r hook; do
        if ! bash -c "$hook"; then
            echo "[${RED}ERROR${RESET}] Hook failed: $hook"
            return 1
        fi
    done <<< "$hooks"

    return 0
}

compare_files() {
    local expected_file=$1
    local actual_file=$2

    exec 3<"$expected_file"
    exec 4<"$actual_file"

    while true;
    do
        IFS= read -r -u 3 expected_line || expected_eof=true
        IFS= read -r -u 4 actual_line || actual_eof=true

        if [ "${expected_eof:-false}" = true ] && [ "${actual_eof:-false}" != true ]; then
            echo "EXPECTED: ${GREEN}<EOF>${RESET}"
            echo "OUTPUT: ${RED}$actual_line${RESET}"
            return 1
        elif [ "${expected_eof:-false}" != true ] && [ "${actual_eof:-false}" = true ]; then
            echo "EXPECTED: ${GREEN}$expected_line${RESET}"
            echo "OUTPUT: ${RED}<EOF>${RESET}"
            return 1
        elif [ "$expected_line" != "$actual_line" ]; then
            echo "EXPECTED: ${GREEN}$expected_line${RESET}"
            echo "OUTPUT: ${RED}$actual_line${RESET}"
            return 1
        else
            echo "OUTPUT: ${GREEN}$actual_line${RESET}"
        fi
        
        if [ "${expected_eof:-false}" = true ] && [ "${actual_eof:-false}" = true ]; then
            break
        fi
    done

    return 0
}

normalize_line_endings() {
    local files=$@
    for file in $files; do
        if [ -f "$file" ]; then
            sed -i 's/\r$//' "$file"
        fi
    done
}

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
        echo "$infile_content" > "$OUTPUT_DIR/${filename}.in"
        infile="$OUTPUT_DIR/${filename}.in"
    else
        echo "[${RED}ERROR${RESET}] Unknown infile type for test $test_id"
        fail=$((fail + 1))
        continue
    fi

    echo "${BOLD}${YELLOW}Test ID: $test_id $test_spec_file${RESET}"

    pre_hooks_type=$( yq -r '.hooks.pre.type // "none"' "$test_spec_file" )
    if [ "$pre_hooks_type" != "none" ]; then
        echo "Running pre-test hooks..."
        if ! run_hooks "$pre_hooks_type" '.hooks.pre'; then
            echo "[${RED}ERROR${RESET}] Pre test hooks failed for test $test_id"
            continue
        fi
    fi

    args=$(yq -r '.args // [] | if type=="string" then . else .[] end' "$test_spec_file")
    set +e
    status=0
    start_time=$(date +%s%3N)
    echo "Runing test..."
    "$BIN" $args < "$infile" > "$output_out" 2> "$output_err"
    status=$?
    end_time=$(date +%s%3N)
    duration=$((end_time - start_time))
    if [ $duration -ge 1000 ]; then
        dur_fmt="$((duration/1000)).$((duration%1000))s"
    else
        dur_fmt="${duration}ms"
    fi
    set -e

    post_hooks_type=$( yq -r '.hooks.post.type // "none"' "$test_spec_file" )
    if [ "$post_hooks_type" != "none" ]; then
        echo "Runing post test hooks..."
        if ! run_hooks "$post_hooks_type" '.hooks.post'; then
            echo "[${RED}ERROR${RESET}] Post test hooks failed for test $test_id"
            continue
        fi
    fi

    ok=1
    
    # Compare stdout
    echo "Comparing stdout..."
    if [ $(yq -r '.expected_stdout.type' "$test_spec_file") == "file" ]; then
        expected_out=$( yq -r '.expected_stdout.file' "$test_spec_file" )
    elif [ $(yq -r '.expected_stdout.type' "$test_spec_file") == "inline" ]; then
        expected_out="$TMP_DIR/${filename}.expected.out"
        yq -r '.expected_stdout.inline | if type=="string" then . else .[] end' "$test_spec_file" > $expected_out
    else
        echo "[${YELLOW}WARN${RESET}] Unknown expected_stdout type for $filename"
        ok=0
    fi

    if [ $ok -eq 1 ]; then
        if [ -f "$expected_out" ]; then
            normalize_line_endings "$expected_out" "$output_out"
            if ! compare_files "$expected_out" "$output_out"; then
                ok=0
            fi
        else
            echo "[${YELLOW}WARN${RESET}] No expected stdout file for $filename"
            ok=0
        fi
    fi

    # Compare stderr
    if [ $(yq -r '.expected_stderr.type' "$test_spec_file") == "file" ]; then
        expected_err=$( yq -r '.expected_stderr.file' "$test_spec_file" )
    elif [ $(yq -r '.expected_stderr.type' "$test_spec_file") == "inline" ]; then
        expected_err="$TMP_DIR/${filename}.expected.err"
        yq -r '.expected_stderr.inline | if type=="string" then . else .[] end' "$test_spec_file" > $expected_err
    else
        echo "[${YELLOW}WARN${RESET}] Unknown expected_stderr type for $filename"
        ok=0
    fi

    if [ $ok -eq 1 ]; then
        echo "Comparing stderr..."
        if [ -f "$expected_err" ]; then
            normalize_line_endings "$expected_err" "$output_err"
            if ! compare_files "$expected_err" "$output_err"; then
                ok=0
            fi
        else
            echo "[${YELLOW}WARN${RESET}] No expected stderr file for $filename"
            ok=0
        fi
    fi

    # Compare exit code
    expected_code=$( yq -r '.expected_ec' "$test_spec_file" )
    if [ $ok -eq 1 ]; then
        echo "Comparing exit code..."
        if [ "$expected_code" != "null" ]; then
            if [ "$status" -ne "$expected_code" ]; then
                echo "Exit code mismatch"
                echo "EXPECTED: ${GREEN}$expected_code${RESET}"
                echo "OUTPUT: ${RED}$status${RESET}"
                ok=0
            else
                echo "Exit code: ${GREEN}$status${RESET}"
            fi
        else
            echo "[${YELLOW}WARN${RESET}] No expected exit code for $filename"
            ok=0
        fi
    fi

    if [ $ok -eq 1 ]; then
        echo "[${GREEN}PASS${RESET}] Test $test_id: PASS"
        pass=$((pass + 1))
        results+=("$test_id|${dur_fmt}|PASS")
    else
        echo "[${RED}FAIL${RESET}] Test $test_id: FAIL"
        fail=$((fail + 1))
        results+=("$test_id|${dur_fmt}|FAIL")
    fi
done

rm -rf $TMP_DIR

echo
echo "+----------------------+------------+--------+"
echo "| ${YELLOW}Test ID${RESET}              | ${YELLOW}Duration${RESET}   | ${YELLOW}Status${RESET} |"
echo "+----------------------+------------+--------+"
for r in "${results[@]}"; do
    IFS="|" read -r id dur stat <<< "$r"

    padded_stat=$(printf "%-6s" "$stat")
    if [ "$stat" = "PASS" ]; then
        colored_stat="${GREEN}${padded_stat}${RESET}"
    else
        colored_stat="${RED}${padded_stat}${RESET}"
    fi

    printf "| %-20s | %-10s | %s |\n" "$id" "$dur" "$colored_stat"
done
echo "+----------------------+------------+--------+"
echo "Passed: $pass, Failed: $fail"
