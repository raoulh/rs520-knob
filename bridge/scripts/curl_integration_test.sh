#!/usr/bin/env bash
#
# Integration tests against real RS520 device using curl.
# Usage: bash curl_integration_test.sh [RS520_IP]
#
set -euo pipefail

RS520="${1:-192.168.30.135}"
BASE="https://${RS520}:9283"
CURL="curl -sk --connect-timeout 5 --max-time 10"
PASS=0
FAIL=0

pass() { PASS=$((PASS+1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL+1)); echo "  ✗ $1: $2"; }

echo "=== RS520 Direct API Tests (${RS520}) ==="
echo ""

# 1. Device name
echo "→ POST /device_name"
RESP=$($CURL -X POST -H "Content-Type: application/json" "${BASE}/device_name" 2>&1) && pass "/device_name" || fail "/device_name" "$RESP"

# 2. Control info
echo "→ GET /get_control_info"
RESP=$($CURL "${BASE}/get_control_info" 2>&1) && pass "/get_control_info" || fail "/get_control_info" "$RESP"

# 3. Current state
echo "→ POST /get_current_state"
RESP=$($CURL -X POST -H "Content-Type: application/json" "${BASE}/get_current_state" 2>&1) && pass "/get_current_state" || fail "/get_current_state" "$RESP"

# 4. Mute state
echo "→ POST /mute.state.get"
RESP=$($CURL -X POST -H "Content-Type: application/json" "${BASE}/mute.state.get" 2>&1) && pass "/mute.state.get" || fail "/mute.state.get" "$RESP"

# 5. Volume set
echo "→ POST /volume (set to 20)"
RESP=$($CURL -X POST -H "Content-Type: application/json" -d '{"volume":20}' "${BASE}/volume" 2>&1) && pass "/volume" || fail "/volume" "$RESP"

# 6. Play/Pause
echo "→ POST /current_play_state (play/pause=17)"
RESP=$($CURL -X POST -H "Content-Type: application/json" -d '{"currentPlayState":17}' "${BASE}/current_play_state" 2>&1) && pass "play_pause" || fail "play_pause" "$RESP"

# Wait a moment
sleep 1

# 7. Play/Pause again (restore state)
echo "→ POST /current_play_state (play/pause=17 restore)"
RESP=$($CURL -X POST -H "Content-Type: application/json" -d '{"currentPlayState":17}' "${BASE}/current_play_state" 2>&1) && pass "play_pause_restore" || fail "play_pause_restore" "$RESP"

echo ""
echo "=== Results ==="
echo "Passed: ${PASS}"
echo "Failed: ${FAIL}"
echo ""

if [ "$FAIL" -gt 0 ]; then
    echo "SOME TESTS FAILED"
    exit 1
fi
echo "ALL TESTS PASSED"
