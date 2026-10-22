#!/usr/bin/env bash
# Usage: run_tests.sh <binary> [suite-glob]
#   env: JOBS=<n> (default nproc)   TEST_TIMEOUT=<sec> (default 30)   VERBOSE=1 (show passing logs)
# Optional spec key:  serial: true   -> test is serialized against other serial tests (shared-state hooks)
set -euo pipefail

BIN=${1:?usage: $0 <binary> [suite-glob]}
SUITE_GLOB=${2:-test/*}
JOBS=${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}
TEST_TIMEOUT=${TEST_TIMEOUT:-30}
VERBOSE=${VERBOSE:-0}

if [ -t 1 ] || [ -n "${GITHUB_ACTIONS:-}" ] || [ -n "${FORCE_COLOR:-}" ]; then
    RED=$'\e[31m'; GREEN=$'\e[32m'; YELLOW=$'\e[33m'; BLUE=$'\e[34m'; BOLD=$'\e[1m'; RESET=$'\e[0m'
else
    RED=""; GREEN=""; YELLOW=""; BLUE=""; BOLD=""; RESET=""
fi

# One yq call per spec: emits shell assignments (safely quoted via @sh).
YQ_FILTER='
def txt(x): if x == null then "" elif (x|type)=="string" then x else (x|map(tostring)|join("\n")) end;
def hook(h): if h.type=="file" then txt(h.file) elif h.type=="inline" then txt(h.inline) else "" end;
@sh "
id=\(.id // "")
skip=\(.skip // false)
serial=\(.serial // false)
in_type=\(.input.type // "")
in_file=\(.input.file // "")
in_inline=\(txt(.input.inline))
args=\(.args // [] | if type=="string" then . else join(" ") end)
pre_type=\(.hooks.pre.type // "none")
pre_body=\(hook(.hooks.pre))
post_type=\(.hooks.post.type // "none")
post_body=\(hook(.hooks.post))
out_kind=\(.expected_stdout.type // "")
out_file=\(.expected_stdout.file // "")
out_inline=\(txt(.expected_stdout.inline))
out_null=\(.expected_stdout.inline == null)
err_kind=\(.expected_stderr.type // "")
err_file=\(.expected_stderr.file // "")
err_inline=\(txt(.expected_stderr.inline))
err_null=\(.expected_stderr.inline == null)
ec=\(.expected_ec)
"'

now_ms() {
    if [ -n "${EPOCHREALTIME:-}" ]; then local t=${EPOCHREALTIME/[.,]/}; echo $((t / 1000))
    else date +%s%3N; fi
}

fmt_dur() { if [ "$1" -ge 1000 ]; then printf '%d.%03ds' $(($1 / 1000)) $(($1 % 1000)); else printf '%dms' "$1"; fi; }

spec_key() { local d; d=$(dirname "$(dirname "$1")"); echo "$(basename "$d")__$(basename "$1" .yaml)"; }

# ---------------------------------------------------------------- worker
run_hook() { # type body
    case $1 in
    file)
        local h
        while IFS= read -r h; do
            [ -z "$h" ] && continue
            echo "Running hook file: $h"
            bash "$h" || { echo "[${RED}ERROR${RESET}] Hook failed: $h"; return 1; }
        done <<<"$2" ;;
    inline)
        [ -z "$2" ] && return 0
        printf '%s\n' "$2" >"$tmp/hook.sh"
        echo "Running inline hook(s):"
        bash "$tmp/hook.sh" || { echo "[${RED}ERROR${RESET}] Inline hook(s) failed"; return 1; } ;;
    *) echo "[${YELLOW}WARN${RESET}] Unknown hooks type: $1"; return 1 ;;
    esac
}

resolve_expected() { # kind file inline isnull dest
    case $1 in
    file) echo "$exp_dir/$2" ;;
    inline)
        if [ "$4" = true ]; then : >"$5"; else printf '%s\n' "$3" >"$5"; fi
        echo "$5" ;;
    *) return 1 ;;
    esac
}

compare() { # label expected actual
    [ -f "$2" ] || { echo "[${YELLOW}WARN${RESET}] No expected $1 file"; return 1; }
    if cmp -s <(sed 's/\r$//' "$2") <(sed 's/\r$//' "$3"); then return 0; fi
    echo "$1 mismatch (${GREEN}-expected${RESET} / ${RED}+actual${RESET}):"
    diff -u --label expected --label actual <(sed 's/\r$//' "$2") <(sed 's/\r$//' "$3") | head -40 || true
    return 1
}

finish() { printf '%s|%s|%s\n' "$id" "${dur:-N/A}" "$1" >"$res"; exit 0; }

worker() {
    set +e; set -u
    local spec=$1
    name=$(basename "$spec" .yaml)
    suite_dir=$(dirname "$(dirname "$spec")")
    key=$(spec_key "$spec")
    res="$RES_DIR/$key.res"
    exec >"$RES_DIR/$key.log" 2>&1

    in_dir="$suite_dir/input"; exp_dir="$suite_dir/expected"; out_dir="$suite_dir/outputs"
    tmp="$suite_dir/tmp/$name"
    mkdir -p "$out_dir" "$tmp"
    trap 'rm -rf "$tmp"' EXIT
    id=$name; dur=""

    vars=$(yq -r "$YQ_FILTER" "$spec") || { echo "[${RED}ERROR${RESET}] Bad spec $spec"; finish FAIL; }
    eval "$vars"
    echo "${BOLD}${YELLOW}Test ID: $id $spec${RESET}"

    if [ "$skip" = true ]; then echo "[${BLUE}SKIP${RESET}] $name"; dur=N/A; finish SKIP; fi
    if [ "$serial" = true ] && command -v flock >/dev/null; then exec 9>"$RES_DIR/serial.lock"; flock 9; fi

    case $in_type in
    file)
        infile="$in_dir/$in_file"
        [ -f "$infile" ] || { echo "[${RED}ERROR${RESET}] Input file $infile missing"; finish FAIL; }
        cp "$infile" "$out_dir/$name.in" ;;
    inline)
        while [[ $in_inline == *$'\n' ]]; do in_inline=${in_inline%$'\n'}; done
        printf '%s\n' "$in_inline" >"$out_dir/$name.in"
        infile="$out_dir/$name.in" ;;
    *) echo "[${RED}ERROR${RESET}] Unknown input type"; finish FAIL ;;
    esac

    if [ "$pre_type" != none ]; then
        run_hook "$pre_type" "$pre_body" || { echo "[${RED}ERROR${RESET}] Pre hooks failed"; finish FAIL; }
    fi

    out="$out_dir/$name.out"; err="$out_dir/$name.err"
    read -ra argv <<<"$args"
    start=$(now_ms)
    timeout "$TEST_TIMEOUT" "$BIN" ${argv[@]+"${argv[@]}"} "$infile" >"$out" 2>"$err"
    status=$?
    dur=$(fmt_dur $(($(now_ms) - start)))
    [ "$status" -eq 124 ] && echo "[${RED}TIMEOUT${RESET}] exceeded ${TEST_TIMEOUT}s"

    if [ "$post_type" != none ]; then
        run_hook "$post_type" "$post_body" || { echo "[${RED}ERROR${RESET}] Post hooks failed"; finish FAIL; }
    fi

    ok=1
    exp_out=$(resolve_expected "$out_kind" "$out_file" "$out_inline" "$out_null" "$tmp/exp.out") \
        || { echo "[${YELLOW}WARN${RESET}] Unknown expected_stdout type"; ok=0; }
    exp_err=$(resolve_expected "$err_kind" "$err_file" "$err_inline" "$err_null" "$tmp/exp.err") \
        || { echo "[${YELLOW}WARN${RESET}] Unknown expected_stderr type"; ok=0; }

    [ $ok -eq 1 ] && { compare stdout "$exp_out" "$out" || ok=0; }
    [ $ok -eq 1 ] && { compare stderr "$exp_err" "$err" || ok=0; }
    if [ $ok -eq 1 ]; then
        if [ "$ec" = null ]; then echo "[${YELLOW}WARN${RESET}] No expected exit code"; ok=0
        elif [ "$status" -ne "$ec" ]; then
            echo "Exit code mismatch: expected ${GREEN}$ec${RESET}, got ${RED}$status${RESET}"; ok=0
        fi
    fi

    if [ $ok -eq 1 ]; then echo "[${GREEN}PASS${RESET}] Test $id ($dur)"; finish PASS
    else echo "[${RED}FAIL${RESET}] Test $id: FAIL"; finish FAIL; fi
}

if [ "${WORKER:-0}" = 1 ]; then worker "$2"; exit 0; fi

# ---------------------------------------------------------------- main
shopt -s nullglob
specs=()
for d in $SUITE_GLOB; do
    [ -d "$d/specs" ] && specs+=("$d"/specs/*.yaml)
done
[ ${#specs[@]} -eq 0 ] && { echo "No specs found under $SUITE_GLOB"; exit 1; }

RES_DIR=$(mktemp -d)
trap 'rm -rf "$RES_DIR"; rm -rf test/*/tmp' EXIT
export BIN RES_DIR YQ_FILTER TEST_TIMEOUT RED GREEN YELLOW BLUE BOLD RESET

echo "Running ${#specs[@]} tests with $JOBS jobs..."
t0=$(now_ms)
printf '%s\0' "${specs[@]}" | WORKER=1 xargs -0 -n1 -P "$JOBS" "$BASH" "$0" "$BIN"
total_ms=$(($(now_ms) - t0))

pass=0; fail=0; skip=0
declare -a rows=()
for spec in "${specs[@]}"; do
    key=$(spec_key "$spec")
    if [ -f "$RES_DIR/$key.res" ]; then IFS='|' read -r id dur stat <"$RES_DIR/$key.res"
    else id=$key; dur=N/A; stat=FAIL; echo "worker crashed" >"$RES_DIR/$key.log"; fi
    rows+=("$id|$dur|$stat")
    case $stat in PASS) pass=$((pass + 1));; SKIP) skip=$((skip + 1));; *) fail=$((fail + 1));; esac

    if [ "$stat" = FAIL ]; then
        [ -n "${GITHUB_ACTIONS:-}" ] && echo "::group::FAIL $id"
        cat "$RES_DIR/$key.log"
        [ -n "${GITHUB_ACTIONS:-}" ] && { echo "::endgroup::"; echo "::error file=$spec::Test $id failed"; }
    elif [ "$VERBOSE" = 1 ]; then cat "$RES_DIR/$key.log"
    elif [ "$stat" = PASS ]; then echo "[${GREEN}PASS${RESET}] $id ($dur)"; fi
done

echo
echo "+----------------------+------------+--------+"
echo "| ${YELLOW}Test ID${RESET}              | ${YELLOW}Duration${RESET}   | ${YELLOW}Status${RESET} |"
echo "+----------------------+------------+--------+"
for r in "${rows[@]}"; do
    IFS='|' read -r id dur stat <<<"$r"
    p=$(printf '%-6s' "$stat")
    case $stat in PASS) c="${GREEN}$p${RESET}";; SKIP) c="${BLUE}$p${RESET}";; *) c="${RED}$p${RESET}";; esac
    printf '| %-20s | %-10s | %s |\n' "$id" "$dur" "$c"
done
echo "+----------------------+------------+--------+"
echo "Passed: $pass, Failed: $fail, Skipped: $skip, Total: $((pass + fail + skip)) in $(fmt_dur "$total_ms")"
[ $fail -eq 0 ]