#!/usr/bin/env bash
set -euo pipefail

mkdir -p cert

openssl req -x509 -nodes -newkey rsa:2048 \
  -keyout cert/server.key \
  -out cert/server.crt \
  -days 365 \
  -subj "/C=CN/ST=Shanghai/L=Shanghai/O=LBL_APP/OU=Backend/CN=localhost"

echo "Generated cert/server.crt and cert/server.key"
