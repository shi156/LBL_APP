#!/usr/bin/env bash
set -euo pipefail

BASE_URL=${BASE_URL:-https://127.0.0.1:1818}
PHONE=${PHONE:-17782275284}
PASS=${PASS:-s12345678}
CURL_NOPROXY=${CURL_NOPROXY:-*}

if ! HEALTH_RESP=$(curl -ks --noproxy "$CURL_NOPROXY" "$BASE_URL/health"); then
  echo "Server is not reachable at ${BASE_URL}." >&2
  echo "Start server first: ./build/lbl_server ./config/app.ini" >&2
  exit 1
fi

echo "$HEALTH_RESP"
echo

curl -k --noproxy "$CURL_NOPROXY" -X POST "$BASE_URL/api/v1/auth/register" \
  -H "Content-Type: application/json" \
  -d "{\"phone\":\"$PHONE\",\"password\":\"$PASS\",\"confirm_password\":\"$PASS\"}"
echo

LOGIN_RESP=$(curl -ks --noproxy "$CURL_NOPROXY" -X POST "$BASE_URL/api/v1/auth/login" \
  -H "Content-Type: application/json" \
  -d "{\"phone\":\"$PHONE\",\"password\":\"$PASS\"}")

echo "$LOGIN_RESP"
TOKEN=$(echo "$LOGIN_RESP" | sed -n 's/.*"access_token":"\([^"]*\)".*/\1/p')

if [[ -z "$TOKEN" ]]; then
  echo "Login did not return token" >&2
  exit 1
fi

curl -k --noproxy "$CURL_NOPROXY" "$BASE_URL/api/v1/users/me" -H "Authorization: Bearer $TOKEN"
echo
