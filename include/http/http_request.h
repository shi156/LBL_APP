#pragma once

#include <string>
#include <unordered_map>

namespace lbl {

// 解析后的 HTTP 请求对象。
struct HttpRequest {
  std::string method;
  std::string path;
  std::string version;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

}  // namespace lbl
