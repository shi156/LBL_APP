#include "security/token_service.h"

#include <openssl/rand.h>

#include <array>

#include "utils/string_utils.h"

namespace lbl {

// 生成 32 字节随机数并编码成十六进制 token。
bool TokenService::generateToken(std::string& out_token) const {
  std::array<unsigned char, 32> raw{};
  if (RAND_bytes(raw.data(), static_cast<int>(raw.size())) != 1) {
    return false;
  }
  out_token = utils::bytesToHex(raw.data(), raw.size());
  return true;
}

}  // namespace lbl
