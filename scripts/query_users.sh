#!/usr/bin/env bash
set -euo pipefail

# 用法示例：
#   ./scripts/query_users.sh
#   DB_USER=app_user DB_PASS=app_pass DB_NAME=app_db ./scripts/query_users.sh

DB_HOST=${DB_HOST:-127.0.0.1}
DB_PORT=${DB_PORT:-3306}
DB_USER=${DB_USER:-app_user}
DB_PASS=${DB_PASS:-app_pass}
DB_NAME=${DB_NAME:-app_db}
LIMIT=${LIMIT:-50}

mysql -h"${DB_HOST}" -P"${DB_PORT}" -u"${DB_USER}" -p"${DB_PASS}" "${DB_NAME}" <<SQL
SELECT id, phone, status, created_at, updated_at
FROM users
ORDER BY id DESC
LIMIT ${LIMIT};
SQL
