#!/usr/bin/env bash
#
# Spins up the exchange server plus N bot clients trading M actions
#
# Usage:
#   ./launch_multi_client.sh [num_clients] [num_actions] [orders_per_client] [options]
#
# Options:
#   --h, --help                           Show this help message and exit
#   --terminals                           Open each client in a separate macOS Terminal window (required here)
#   --reload_prices                       Refetch + save the yahoo finance data (keeping only the tickers in `tickers.txt`)
#   --period N                            Type of yahoo finance period to fetch (default: 5y)
#   --interval N                          Type of yahoo finance interval to fetch (default: 1d)
#   --history_length N                    Minimum number of rows of price history to keep (default: 100)
#   --use_real_prices                     Use real prices from the database (default: False)
#   --continue_session                    Don't reset the database, continue from the last session (default: False)
#   --clients_balance                     Starting balance for each client (default: 10000)
#   --command-delay N                     Delay between commands from the same client (default: 0.5s)
#   --random-delay N                      Random jitter added to command delay (default: 0.2s)
#   --pre_open_time_delay N               Delay before the market opens (default: 1000ms)
#   --open_time_delay N                   Delay during market open (default: 1000ms)
#   --continuous_trading_time_delay N     Delay during continuous trading (default: 30000ms)
#   --continuous_trading_loop_duration N  Duration of continuous trading loop (default: 1000ms)
#   --pre_close_time_delay N              Delay before market close (default: 1000ms)
#   --trigger_poll_interval N             Interval between trigger polls (default: 500ms)
#
# Examples:
#
#   # 2 clients, 1 actions, 10 orders each, non realistic prices
#   ./launch_multi_client.sh --terminals 
#
#   # 2 clients, 1 actions, 10 orders each, continuing from the last session (database not reset)
#   ./launch_multi_client.sh --terminals --continue_session
#
#   # 20 clients, 6 actions, 30 orders each, reloading the yahoo finance data but not using them
#   ./launch_multi_client.sh 20 6 30 --terminals --reload_prices --period 5y --interval 1d --history_length 10
#
#   # Normal mode with 1s between orders + up to 200ms jitter
#   ./launch_multi_client.sh 20 6 30 --terminals --use_real_prices --command-delay 1.0 --random-delay 0.2 --clients_balance 100000
#
# Both modes redirect each client's output to scripts/logs/clientN.log
# In --terminals mode the same output is also shown live in that client's window
#
set -euo pipefail

# ============================================================
# Configuration (defaults, may be overridden by flags/positional args below)
# ============================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_SQL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

NUM_CLIENTS=2
NUM_ACTIONS=1
ORDERS_PER_CLIENT=1
CLIENTS_BALANCE=10000
CONTINUE_SESSION="false"
USE_REAL_PRICES="false"
USE_TERMINALS="false"
RELOAD_PRICES="false"
PERIOD="5y"
INTERVAL="1d"
HISTORY_LENGTH=100

COMMAND_DELAY="0.5"
RANDOM_DELAY="0.2"

PRE_OPEN_TIME_DELAY="1000"
OPEN_TIME_DELAY="1000"
CONTINUOUS_TRADING_TIME_DELAY="30000"
CONTINUOUS_TRADING_LOOP_DURATION="1000"
PRE_CLOSE_TIME_DELAY="1000"
TRIGGER_POLL_INTERVAL="500"

# ============================================================
# Parse options
# ============================================================
positional_index=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            sed -n '2,32p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        --terminals)
            USE_TERMINALS="true"
            shift
            ;;
        --reload_prices)
            RELOAD_PRICES="true"
            shift
            ;;
        --period)
            PERIOD="${2:?--period needs a value}"
            shift 2
            ;;
        --interval)
            INTERVAL="${2:?--interval needs a value}"
            shift 2
            ;;
        --history_length)
            HISTORY_LENGTH="${2:?--history_length needs a value}"
            shift 2
            ;;
        --use_real_prices)
            USE_REAL_PRICES="true"
            shift
            ;;
        --continue_session)
            CONTINUE_SESSION="true"
            shift
            ;;
        --clients_balance)
            CLIENTS_BALANCE="${2:?--clients_balance needs a value}"
            shift 2
            ;;
        --command-delay)
            COMMAND_DELAY="${2:?--command-delay needs a value}"
            shift 2
            ;;
        --random-delay)
            RANDOM_DELAY="${2:?--random-delay needs a value}"
            shift 2
            ;;
        --pre_open_time_delay)
            PRE_OPEN_TIME_DELAY="${2:?--pre_open_time_delay needs a value}"
            shift 2
            ;;
        --open_time_delay)
            OPEN_TIME_DELAY="${2:?--open_time_delay needs a value}"
            shift 2
            ;;
        --continuous_trading_time_delay)
            CONTINUOUS_TRADING_TIME_DELAY="${2:?--continuous_trading_time_delay needs a value}"
            shift 2
            ;;
        --continuous_trading_loop_duration)
            CONTINUOUS_TRADING_LOOP_DURATION="${2:?--continuous_trading_loop_duration needs a value}"
            shift 2
            ;;
        --pre_close_time_delay)
            PRE_CLOSE_TIME_DELAY="${2:?--pre_close_time_delay needs a value}"
            shift 2
            ;;
        --trigger_poll_interval)
            TRIGGER_POLL_INTERVAL="${2:?--trigger_poll_interval needs a value}"
            shift 2
            ;;
        --*)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
        *)
            case "${positional_index}" in
                0) NUM_CLIENTS="$1" ;;
                1) NUM_ACTIONS="$1" ;;
                2) ORDERS_PER_CLIENT="$1" ;;
                *)
                    echo "Unexpected extra argument: $1" >&2
                    exit 1
                    ;;
            esac
            positional_index=$((positional_index + 1))
            shift
            ;;
    esac
done

# fail fast with a clear message instead of a cryptic "seq"/"server.x init" error later
for name_value in "NUM_CLIENTS:${NUM_CLIENTS}" "NUM_ACTIONS:${NUM_ACTIONS}" "ORDERS_PER_CLIENT:${ORDERS_PER_CLIENT}"; do
    name="${name_value%%:*}"
    value="${name_value#*:}"
    if ! [[ "${value}" =~ ^[0-9]+$ ]] || [[ "${value}" -le 0 ]]; then
        echo "Error: ${name} must be a positive integer, got '${value}'" >&2
        exit 1
    fi
done

# ============================================================
# Directories
# ============================================================
LOG_DIR="${SRC_SQL_DIR}/scripts/logs"
CMD_DIR="${SRC_SQL_DIR}/scripts/generated_commands"
rm -rf "${LOG_DIR}" "${CMD_DIR}"
mkdir -p "${LOG_DIR}" "${CMD_DIR}"

# ============================================================
# Display configuration
# ============================================================
echo ""
echo "=============================================="
echo " Multi-client exchange test"
echo "=============================================="
echo " Clients:          ${NUM_CLIENTS}"
echo " Actions:          ${NUM_ACTIONS}"
echo " Orders/client:    ${ORDERS_PER_CLIENT}"
echo " Starting balance: ${CLIENTS_BALANCE}"
echo " Command delay:    ${COMMAND_DELAY}s"
echo " Random delay:     ${RANDOM_DELAY}s"
echo " Terminals:        ${USE_TERMINALS}"
echo " Use real prices:  ${USE_REAL_PRICES}"
echo " Continue session: ${CONTINUE_SESSION}"
echo "=============================================="
echo ""

if [[ "${USE_TERMINALS}" == "true" ]] && ! command -v osascript >/dev/null 2>&1; then
    echo "Error: --terminals was requested but 'osascript' is not available (macOS only). Falling back to headless mode." >&2
    USE_TERMINALS="false"
fi

# ============================================================
# Build
# ============================================================
echo "==> Building server.x and client_account.x"
(cd "${SRC_SQL_DIR}" && make realclean && make)

# ============================================================
# Reset database
# ============================================================
echo "==> Resetting the database (only if --continue_session is not set)"
if [[ "${CONTINUE_SESSION}" == "true" ]]; then
    echo "==> Continuing from the last session (database not reset)"
else
    echo "==> Resetting the database (server.x reset)"
    (cd "${SRC_SQL_DIR}" && ./server.x reset)
fi

# ============================================================
# Reload the yahoo finance data (if requested)
# ============================================================
if [[ "${RELOAD_PRICES}" == "true" ]]; then
    echo "==> Refetching and saving the yahoo finance data (Data/fetch_data.py)"
    (cd "${SRC_SQL_DIR}/../Data" && python3 fetch_data.py --tickers-file tickers.txt --period "${PERIOD}" --interval "${INTERVAL}" --output test)
    echo "==> Preprocessing the data (Data/preprocess.py)"
    (cd "${SRC_SQL_DIR}/../Data" && python3 preprocess.py --keep_only_fetched --input test --max-rows "${HISTORY_LENGTH}")
fi

# ============================================================
# Enter actions prices in the database
# ============================================================
if [[ "${USE_REAL_PRICES}" == "true" && "${CONTINUE_SESSION}" != "true" ]]; then
    echo "==> Entering actions prices in the database"
    # It suppose that "../Data/Datasets/processed/all_prices.csv" exists, and that "../Data/fetch_data.py", "../Data/preprocess.py" and "../Data/feature_engineering.py" have been run before (with the right CLI arguments) to generate the CSV file
    # If not, you can run them manually before running this script
    #if [[ ! -x "${SRC_SQL_DIR}/../Data/Stock_Market_App.db" ]]; then
    #    chmod +x "${SRC_SQL_DIR}/../Data/Stock_Market_App.db"
    #fi
    (cd "${SRC_SQL_DIR}" && ../Data/enter_in_database.py --db ../Data/Stock_Market_App.db --input ../Data/Datasets/processed/all_prices.csv)
else
    echo "==> Using synthetic prices (no action price entry)"
fi

# ============================================================
# Seed clients and actions
# ============================================================
echo "==> Seeding ${NUM_CLIENTS} clients (with ${CLIENTS_BALANCE} starting balance)"
if [[ "${CONTINUE_SESSION}" == "true" ]]; then
    echo "==> Continuing from the last session (server.x init skipped)"
    echo "==> Note: the number of clients provided should match the existing database clients"
    echo "==> Note: the number of actions provided should match the existing database actions"
    echo "==> Note: the starting balance provided will be ignored (existing balances remain)"
    echo "==> Unfortunately, there is no way currently coded to verify that the provided numbers match the existing database, numbers need to match"
else
    USE_REAL_PRICES_FLAG="0"
    if [[ "${USE_REAL_PRICES}" == "true" ]]; then
        USE_REAL_PRICES_FLAG="1"
    fi
    (cd "${SRC_SQL_DIR}" && ./server.x init "${NUM_CLIENTS}" "${CLIENTS_BALANCE}" "${USE_REAL_PRICES_FLAG}" "${NUM_ACTIONS}")
fi

# `server.x init` now generates a random 12-char password per synthetic client (instead of the old fixed "123" for everyone) 
# and writes the plaintext alongside the DB so this script can log clients in correctly
CREDENTIALS_FILE="${SRC_SQL_DIR}/../Data/generated_credentials.csv"
if [[ ! -f "${CREDENTIALS_FILE}" ]]; then
    echo "Error: expected generated credentials at ${CREDENTIALS_FILE} but it wasn't created by 'server.x init'" >&2
    exit 1
fi

# looks up the password for a given numeric client id from the CSV (client_id,name,password)
client_password() {
    local client_id="$1"
    awk -F',' -v cid="${client_id}" '$1==cid {print $3; found=1} END{exit !found}' "${CREDENTIALS_FILE}"
}

# ============================================================
# Query the actions prices
# ============================================================
if [[ "${USE_REAL_PRICES}" == "true" ]]; then
    ACTION_PRICE_PAIRS="$(python3 "${SCRIPT_DIR}/query_action_prices.py" --db ../../Data/Stock_Market_App.db | tr '\n' ' ')"
    if [[ -z "${ACTION_PRICE_PAIRS// /}" ]]; then
        echo "No actions with price history found in ../../Data/Stock_Market_App.db after loading -- aborting" >&2
        exit 1
    fi
    echo "==> Using real action prices: ${ACTION_PRICE_PAIRS}"  
fi
sleep 5

# ============================================================
# Start server
# ============================================================
echo "==> Starting the server"
(cd "${SRC_SQL_DIR}" && ./server.x play "${PRE_OPEN_TIME_DELAY}" "${OPEN_TIME_DELAY}" "${CONTINUOUS_TRADING_TIME_DELAY}" "${CONTINUOUS_TRADING_LOOP_DURATION}" "${PRE_CLOSE_TIME_DELAY}" "${TRIGGER_POLL_INTERVAL}" > "${LOG_DIR}/server.log" 2>&1 &)
SERVER_PID_FILE="${LOG_DIR}/server.pid"
# server.x doesn't print its own PID, wait briefly and find the process
sleep 1
pgrep -n -f "${SRC_SQL_DIR}/server.x play ${PRE_OPEN_TIME_DELAY} ${OPEN_TIME_DELAY} ${CONTINUOUS_TRADING_TIME_DELAY} ${CONTINUOUS_TRADING_LOOP_DURATION} ${PRE_CLOSE_TIME_DELAY} ${TRIGGER_POLL_INTERVAL}" > "${SERVER_PID_FILE}" || true

# ============================================================
# Wait for server
# ============================================================
echo "==> Waiting for the server to accept connections"
SERVER_READY="false"
for _ in $(seq 1 20); do
    if grep -q "Waiting for connexion" "${LOG_DIR}/server.log" 2>/dev/null; then
        SERVER_READY="true"
        break
    fi
    sleep 0.5
done
if [[ "${SERVER_READY}" != "true" ]]; then
    echo "WARNING: server readiness message was not detected."
    echo "         Check ${LOG_DIR}/server.log"
fi

# ============================================================
# Generate client command files
# ============================================================
echo "==> Generating order sequences"
for i in $(seq 1 "${NUM_CLIENTS}"); do
    CMD_FILE="${CMD_DIR}/client${i}_commands.txt"
    if [[ "${USE_REAL_PRICES}" == "true" ]]; then
        python3 "${SCRIPT_DIR}/generate_client_orders.py" \
            --action-prices ${ACTION_PRICE_PAIRS} \
            --num-orders "${ORDERS_PER_CLIENT}" \
            --seed "${i}" \
            --display-every 0 \
            > "${CMD_FILE}"
            # --exit-at-end \
    else
        python3 "${SCRIPT_DIR}/generate_client_orders.py" \
            --num-actions "${NUM_ACTIONS}" \
            --num-orders "${ORDERS_PER_CLIENT}" \
            --seed "${i}" \
            --display-every 0 \
            > "${CMD_FILE}"
            # --exit-at-end \
    fi
done

# a single random-ish seed component per invocation, PID + bash's own $RANDOM 
# so back-to-back calls in the same wall-clock second don't all draw the same "random" jitter 
# (mixing $$ with $RANDOM keeps this portable to macOS's stock awk, unlike gawk-only PROCINFO)
random_delay_seconds() {
    local base="$1" spread="$2"
    awk -v a="${base}" -v b="${spread}" -v seed="$$${RANDOM}" \
        'BEGIN{srand(seed); printf "%.3f", a + (b > 0 ? rand() * b : 0)}'
}

# ============================================================
# Launch clients
# ============================================================
echo "==> Launching ${NUM_CLIENTS} clients"
CLIENT_PIDS=()
for i in $(seq 1 "${NUM_CLIENTS}"); do
    CMD_FILE="${CMD_DIR}/client${i}_commands.txt"
    LOG_FILE="${LOG_DIR}/client${i}.log"

    CLIENT_PASSWORD="$(client_password "${i}")"
    if [[ -z "${CLIENT_PASSWORD}" ]]; then
        echo "Error: no password found for Client${i} in ${CREDENTIALS_FILE}" >&2
        exit 1
    fi

    # --------------------------------------------------------
    # macOS Terminal mode
    # --------------------------------------------------------
    if [[ "${USE_TERMINALS}" == "true" ]]; then
        RUNNER_FILE="${CMD_DIR}/client${i}_runner.sh"
        {
            echo "#!/usr/bin/env bash"
            echo "cd '${SRC_SQL_DIR}'"
            echo "while IFS= read -r command; do"
            echo "    echo \"\${command}\""
            echo "    sleep \$(awk -v a='${COMMAND_DELAY}' -v b='${RANDOM_DELAY}' -v seed=\"\$\$\${RANDOM}\" 'BEGIN{srand(seed); printf \"%.3f\", a + (b > 0 ? rand() * b : 0)}')"
            echo "done < '${CMD_FILE}' | ./client_account.x 'Client${i}' '${CLIENT_PASSWORD}' 2>&1 | tee '${LOG_FILE}'"
        } > "${RUNNER_FILE}"
        chmod +x "${RUNNER_FILE}"
 
        osascript -e "
            tell application \"Terminal\"
                do script \"bash '${RUNNER_FILE}'\"
            end tell
        " >/dev/null

    # --------------------------------------------------------
    # Headless mode
    # --------------------------------------------------------
    else
        (
            cd "${SRC_SQL_DIR}"
            while IFS= read -r command; do
                echo "${command}"
                sleep "$(random_delay_seconds "${COMMAND_DELAY}" "${RANDOM_DELAY}")"
            done < "${CMD_FILE}" | ./client_account.x "Client${i}" "${CLIENT_PASSWORD}" > "${LOG_FILE}" 2>&1
        ) &
        CLIENT_PIDS+=($!)
    fi
done

# ============================================================
# Wait for clients to finish
# ============================================================
echo ""
if [[ "${USE_TERMINALS}" == "true" ]]; then
    echo "==> ${NUM_CLIENTS} clients launched in separate Terminal windows"
    echo "==> Waiting for them to finish (polling their logs)..."
    for i in $(seq 1 "${NUM_CLIENTS}"); do
        LOG_FILE="${LOG_DIR}/client${i}.log"
        # up to ~2 minutes per client; adjust if your order counts/delays run longer than that
        for _ in $(seq 1 1200); do
            if grep -qE "Connexion closed by the server|Connection closed by the server" "${LOG_FILE}" 2>/dev/null; then
                break
            fi
            sleep 0.1
        done
    done
else
    echo "==> Waiting for all headless clients to finish..."
    wait "${CLIENT_PIDS[@]}" 2>/dev/null || true
fi

# ============================================================
# Done
# ============================================================
echo ""
echo "=============================================="
echo " Test complete"
echo "=============================================="
echo " Client logs: ${LOG_DIR}/clientN.log"
echo " Server log:  ${LOG_DIR}/server.log"
echo ""

# a much more reliable way to confirm actual concurrency than watching N terminal windows race
# every executed transaction is timestamped to the millisecond in the server log
# so overlapping or tightly-clustered timestamps across different buyer/seller ids are your proof of concurrent execution
if grep -q "Type: TRANSACTION" "${LOG_DIR}/server.log" 2>/dev/null; then
    echo ""
    echo "First transactions observed (timestamps, for spotting overlap/concurrency):"
    grep "Type: TRANSACTION" "${LOG_DIR}/server.log" | sed -n '1,20p'
fi
 
echo "=============================================="