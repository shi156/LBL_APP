#pragma once

#include <string>

namespace lbl {

class PasswordHasher {
 public:
  // 构造密码哈希器，iterations 为 PBKDF2 迭代次数。
  explicit PasswordHasher(int iterations);

  // 生成可落库密码记录，格式：pbkdf2_sha256$<iterations>$<salt_hex>$<hash_hex>
  // password: 明文密码；out_record: 输出哈希记录。
  bool createPasswordRecord(const std::string& password, std::string& out_record) const;
  // 校验明文密码与数据库存储记录是否匹配。
  // password: 用户输入密码；stored_record: 数据库存储串。
  bool verifyPassword(const std::string& password, const std::string& stored_record) const;

 private:
  int iterations_;
};

}  // namespace lbl
