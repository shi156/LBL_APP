#include "db/database.h"

#include <chrono>
#include <sstream>

namespace lbl {

// 构造数据库对象。
Database::Database() = default;

// 析构时关闭连接。
Database::~Database() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (conn_ != nullptr) {
    mysql_close(conn_);
    conn_ = nullptr;
  }
}

// 建立 MySQL 长连接。
bool Database::connect(const DatabaseConfig& cfg, std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (conn_ != nullptr) {
    mysql_close(conn_);
    conn_ = nullptr;
  }

  conn_ = mysql_init(nullptr);
  if (conn_ == nullptr) {
    error = "mysql_init failed";
    return false;
  }

  // charset 可通过配置修改，例如 utf8mb4。
  mysql_options(conn_, MYSQL_SET_CHARSET_NAME, cfg.charset.c_str());

  MYSQL* result = mysql_real_connect(conn_,
                                     cfg.host.c_str(),
                                     cfg.user.c_str(),
                                     cfg.password.c_str(),
                                     cfg.database.c_str(),
                                     cfg.port,
                                     nullptr,
                                     0);
  if (result == nullptr) {
    error = std::string("mysql_real_connect failed: ") + mysql_error(conn_);
    mysql_close(conn_);
    conn_ = nullptr;
    return false;
  }

  return true;
}

// 初始化表结构（重复执行不会破坏已有表）。
bool Database::initSchema(std::string& error) {
  const std::string create_users =
      "CREATE TABLE IF NOT EXISTS users ("
      "id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,"
      "phone VARCHAR(20) NOT NULL,"
      "password_hash VARCHAR(255) NOT NULL,"
      "password_algo VARCHAR(20) NOT NULL DEFAULT 'pbkdf2_sha256',"
      "status TINYINT NOT NULL DEFAULT 1,"
      "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
      "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
      "UNIQUE KEY uk_phone (phone)"
      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

  const std::string create_tokens =
      "CREATE TABLE IF NOT EXISTS auth_tokens ("
      "id BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,"
      "user_id BIGINT UNSIGNED NOT NULL,"
      "token VARCHAR(128) NOT NULL,"
      "expires_at DATETIME NOT NULL,"
      "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
      "UNIQUE KEY uk_token (token),"
      "KEY idx_user_id (user_id),"
      "CONSTRAINT fk_auth_tokens_user_id FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE"
      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

  if (!exec(create_users, error)) {
    return false;
  }
  return exec(create_tokens, error);
}

// 创建用户记录。
bool Database::createUser(const std::string& phone,
                          const std::string& password_hash,
                          std::uint64_t& out_user_id,
                          std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (conn_ == nullptr) {
    error = "Database not connected";
    return false;
  }

  const auto phone_escaped = escape(phone);
  const auto hash_escaped = escape(password_hash);

  const std::string sql =
      "INSERT INTO users(phone, password_hash, password_algo) VALUES('" + phone_escaped +
      "', '" + hash_escaped + "', 'pbkdf2_sha256');";

  if (mysql_query(conn_, sql.c_str()) != 0) {
    const auto err_code = mysql_errno(conn_);
    if (err_code == 1062) {
      error = "phone already registered";
      return false;
    }
    error = std::string("createUser mysql_query failed: ") + mysql_error(conn_);
    return false;
  }

  out_user_id = mysql_insert_id(conn_);
  return true;
}

// 按手机号查询用户。
std::optional<UserRecord> Database::findUserByPhone(const std::string& phone, std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (conn_ == nullptr) {
    error = "Database not connected";
    return std::nullopt;
  }

  const std::string sql =
      "SELECT id, phone, password_hash, status FROM users WHERE phone='" + escape(phone) +
      "' LIMIT 1;";
  if (mysql_query(conn_, sql.c_str()) != 0) {
    error = std::string("findUserByPhone mysql_query failed: ") + mysql_error(conn_);
    return std::nullopt;
  }

  MYSQL_RES* res = mysql_store_result(conn_);
  if (res == nullptr) {
    error = std::string("findUserByPhone mysql_store_result failed: ") + mysql_error(conn_);
    return std::nullopt;
  }

  MYSQL_ROW row = mysql_fetch_row(res);
  if (row == nullptr) {
    mysql_free_result(res);
    return std::nullopt;
  }

  UserRecord record;
  try {
    record.id = std::stoull(row[0] != nullptr ? row[0] : "0");
  } catch (...) {
    mysql_free_result(res);
    error = "Invalid user id from database";
    return std::nullopt;
  }
  record.phone = row[1] != nullptr ? row[1] : "";
  record.password_hash = row[2] != nullptr ? row[2] : "";
  try {
    record.status = std::stoi(row[3] != nullptr ? row[3] : "0");
  } catch (...) {
    record.status = 0;
  }

  mysql_free_result(res);
  return record;
}

// 保存 token 并设置过期时间。
bool Database::saveToken(std::uint64_t user_id,
                         const std::string& token,
                         int expires_in_seconds,
                         std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (conn_ == nullptr) {
    error = "Database not connected";
    return false;
  }

  const std::string token_escaped = escape(token);

  // 一个用户可并存多个 token，便于多端同时登录。
  const std::string sql =
      "INSERT INTO auth_tokens(user_id, token, expires_at) VALUES(" + std::to_string(user_id) +
      ", '" + token_escaped + "', DATE_ADD(NOW(), INTERVAL " +
      std::to_string(expires_in_seconds) + " SECOND));";

  if (mysql_query(conn_, sql.c_str()) != 0) {
    error = std::string("saveToken mysql_query failed: ") + mysql_error(conn_);
    return false;
  }

  return true;
}

// 按 token 反查用户，用于鉴权接口。
std::optional<UserRecord> Database::findUserByToken(const std::string& token, std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (conn_ == nullptr) {
    error = "Database not connected";
    return std::nullopt;
  }

  const std::string sql =
      "SELECT u.id, u.phone, u.password_hash, u.status "
      "FROM auth_tokens t INNER JOIN users u ON t.user_id = u.id "
      "WHERE t.token='" +
      escape(token) + "' AND t.expires_at > NOW() LIMIT 1;";

  if (mysql_query(conn_, sql.c_str()) != 0) {
    error = std::string("findUserByToken mysql_query failed: ") + mysql_error(conn_);
    return std::nullopt;
  }

  MYSQL_RES* res = mysql_store_result(conn_);
  if (res == nullptr) {
    error = std::string("findUserByToken mysql_store_result failed: ") + mysql_error(conn_);
    return std::nullopt;
  }

  MYSQL_ROW row = mysql_fetch_row(res);
  if (row == nullptr) {
    mysql_free_result(res);
    return std::nullopt;
  }

  UserRecord record;
  try {
    record.id = std::stoull(row[0] != nullptr ? row[0] : "0");
  } catch (...) {
    mysql_free_result(res);
    error = "Invalid user id from database";
    return std::nullopt;
  }
  record.phone = row[1] != nullptr ? row[1] : "";
  record.password_hash = row[2] != nullptr ? row[2] : "";
  try {
    record.status = std::stoi(row[3] != nullptr ? row[3] : "0");
  } catch (...) {
    record.status = 0;
  }

  mysql_free_result(res);
  return record;
}

// 执行通用 SQL（不返回结果集）。
bool Database::exec(const std::string& sql, std::string& error) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (conn_ == nullptr) {
    error = "Database not connected";
    return false;
  }
  if (mysql_query(conn_, sql.c_str()) != 0) {
    error = std::string("mysql_query failed: ") + mysql_error(conn_);
    return false;
  }
  return true;
}

// 进行 SQL 字符串转义，降低注入风险。
std::string Database::escape(const std::string& value) {
  // 调用方需持有 mutex_，保证 conn_ 线程安全。
  std::string escaped;
  escaped.resize(value.size() * 2 + 1);
  const unsigned long len = mysql_real_escape_string(
      conn_, escaped.data(), value.c_str(), static_cast<unsigned long>(value.size()));
  escaped.resize(len);
  return escaped;
}

}  // namespace lbl
