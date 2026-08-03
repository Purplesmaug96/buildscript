#!/usr/bin/env bash

# Continuously show the last line of the OpenXeChain build log.
# Usage: ./inf-tail.sh [log-file] [refresh-seconds]

LOG_FILE="${1:-build.log}"
REFRESH="${2:-1}"

# Exit cleanly on Ctrl+C
trap 'clear; exit 0' INT

while true; do
    clear
    if [[ -f "${LOG_FILE}" ]]; then
        tail -n 1 "${LOG_FILE}"
    else
        echo "Waiting for ${LOG_FILE} to appear..."
    fi
    sleep "${REFRESH}"
done
