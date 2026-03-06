#include "security/password_hasher.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "utils/string_utils.h"

namespace lbl {
namespace {

constexpr int kSaltLen = 16;
constexpr int kHashLen = 32;

// 按分隔符拆分字符串。
std::vector<std::string> split(const std::string& input, char delimiter) {
  std::vector<std::string> items;
  std::stringstream ss(input);
  std::string item;
  while (std::getline(ss, item, delimiter)) {
    items.push_back(item);
  }
  return items;
}

// 调用 OpenSSL PBKDF2 派生密码哈希。
bool derivePbkdf2(const std::string& password,
                  const unsigned char* salt,
                  int salt_len,
                  int iterations,
                  unsigned char* out,
                  int out_len) {
  return PKCS5_PBKDF2_HMAC(password.c_str(),
                           static_cast<int>(password.size()),
                           salt,
                           salt_len,
                           iterations,
                           EVP_sha256(),
                           out_len,
                           out) == 1;
}

}  // namespace

// 构造哈希器并保存迭代次数参数。
PasswordHasher::PasswordHasher(int iterations) : iterations_(iterations) {}

// 生成用于数据库存储的密码记录串。
bool PasswordHasher::createPasswordRecord(const std::string& password, std::string& out_record) const {
  if (password.empty()) {
    return false;
  }

  std::array<unsigned char, kSaltLen> salt{};
  if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
    return false;
  }

  std::array<unsigned char, kHashLen> hash{};
  if (!derivePbkdf2(password,
                    salt.data(),
                    static_cast<int>(salt.size()),
                    iterations_,
                    hash.data(),
                    static_cast<int>(hash.size()))) {
    return false;
  }

  out_record = "pbkdf2_sha256$" + std::to_string(iterations_) + "$" +
               utils::bytesToHex(salt.data(), salt.size()) + "$" +
               utils::bytesToHex(hash.data(), hash.size());
  return true;
}

// 校验明文密码是否匹配数据库记录。
bool PasswordHasher::verifyPassword(const std::string& password,
                                    const std::string& stored_record) const {
  const auto parts = split(stored_record, '$');
  if (parts.size() != 4 || parts[0] != "pbkdf2_sha256") {
    return false;
  }

  int iterations = 0;
  try {
    iterations = std::stoi(parts[1]);
  } catch (...) {
    return false;
  }

  const auto salt = utils::hexToBytes(parts[2]);
  const auto expected_hash = utils::hexToBytes(parts[3]);
  if (salt.empty() || expected_hash.empty()) {
    return false;
  }

  std::vector<unsigned char> actual_hash(expected_hash.size());
  if (!derivePbkdf2(password,
                    salt.data(),
                    static_cast<int>(salt.size()),
                    iterations,
                    actual_hash.data(),
                    static_cast<int>(actual_hash.size()))) {
    return false;
  }

  return CRYPTO_memcmp(actual_hash.data(), expected_hash.data(), expected_hash.size()) == 0;
}

}  // namespace lbl
