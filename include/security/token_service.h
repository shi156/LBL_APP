#pragma once

#include <string>

namespace lbl {

class TokenService {
 public:
  // 生成高熵随机 token（十六进制字符串）用于 API 鉴权。
  // out_token: 输出 token。
  bool generateToken(std::string& out_token) const;
};

}  // namespace lbl
