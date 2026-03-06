#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include <mysql/mysql.h>

#include "config/app_config.h"

namespace lbl {

struct UserRecord {
  std::uint64_t id = 0;
  std::string phone;
  std::string password_hash;
  int status = 1;
};

class Database {
 public:
  // 构造数据库对象（此时尚未连接）。
  Database();
  // 析构时关闭 MySQL 连接。
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  // 建立长连接，供请求处理流程复用。
  // cfg: 数据库连接参数；error: 失败错误信息。
  bool connect(const DatabaseConfig& cfg, std::string& error);
  // 初始化项目所需表结构（幂等执行）。
  bool initSchema(std::string& error);

  // 创建用户。
  // phone: 手机号；password_hash: 已哈希密码；out_user_id: 输出新用户 ID。
  bool createUser(const std::string& phone,
                  const std::string& password_hash,
                  std::uint64_t& out_user_id,
                  std::string& error);

  // 按手机号查询用户（登录流程使用）。
  std::optional<UserRecord> findUserByPhone(const std::string& phone, std::string& error);
  // 保存登录 token，并记录过期时间。
  bool saveToken(std::uint64_t user_id,
                 const std::string& token,
                 int expires_in_seconds,
                 std::string& error);
  // 按 token 反查当前用户（鉴权流程使用）。
  std::optional<UserRecord> findUserByToken(const std::string& token, std::string& error);

 private:
  // 执行不需要返回结果集的 SQL（DDL / INSERT / UPDATE）。
  bool exec(const std::string& sql, std::string& error);
  // 对输入字符串做 SQL 转义，防止注入。
  std::string escape(const std::string& value);

  MYSQL* conn_ = nullptr;
  std::mutex mutex_;
};

}  // namespace lbl
