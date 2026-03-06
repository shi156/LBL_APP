#include "http/http_parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <string>

#include "utils/string_utils.h"

namespace lbl {
namespace {

constexpr std::size_t kMaxHeaderBytes = 32 * 1024;
constexpr std::size_t kMaxBodyBytes = 256 * 1024;

// 统一 header key 规范为小写并去除空白。
std::string toHeaderKey(std::string key) {
  key = utils::toLower(utils::trim(std::move(key)));
  return key;
}

// 解析完整 HTTP 原始报文到结构体。
bool parseHttp(const std::string& raw, HttpRequest& out_request, std::string& error) {
  const auto header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    error = "Invalid HTTP payload: header terminator not found";
    return false;
  }

  const auto header_text = raw.substr(0, header_end);
  out_request.body = raw.substr(header_end + 4);

  std::istringstream stream(header_text);
  std::string first_line;
  if (!std::getline(stream, first_line)) {
    error = "Invalid HTTP payload: empty request line";
    return false;
  }

  if (!first_line.empty() && first_line.back() == '\r') {
    first_line.pop_back();
  }

  {
    std::istringstream first_line_stream(first_line);
    if (!(first_line_stream >> out_request.method >> out_request.path >> out_request.version)) {
      error = "Invalid HTTP request line";
      return false;
    }
  }

  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }

    const auto sep = line.find(':');
    if (sep == std::string::npos) {
      continue;
    }

    auto key = toHeaderKey(line.substr(0, sep));
    auto value = utils::trim(line.substr(sep + 1));
    out_request.headers[key] = value;
  }

  return true;
}

// 解析 Content-Length，失败返回 0。
std::size_t contentLength(const HttpRequest& request) {
  auto it = request.headers.find("content-length");
  if (it == request.headers.end()) {
    return 0;
  }

  try {
    return static_cast<std::size_t>(std::stoul(it->second));
  } catch (...) {
    return 0;
  }
}

}  // namespace

// 从 TLS 连接读取并解析 1 个 HTTP 请求。
bool readHttpRequest(SSL* ssl, HttpRequest& out_request, std::string& error) {
  out_request = HttpRequest{};

  std::string raw;
  raw.reserve(4096);

  std::array<char, 4096> buf{};
  std::size_t header_end = std::string::npos;
  std::size_t expected_total = 0;

  while (true) {
    const int bytes = SSL_read(ssl, buf.data(), static_cast<int>(buf.size()));
    if (bytes <= 0) {
      const int ssl_error = SSL_get_error(ssl, bytes);
      error = "SSL_read failed with error code " + std::to_string(ssl_error);
      return false;
    }

    raw.append(buf.data(), static_cast<std::size_t>(bytes));

    // 对总报文长度做上限保护，避免异常大请求拖垮服务。
    if (raw.size() > kMaxHeaderBytes + kMaxBodyBytes) {
      error = "HTTP request too large";
      return false;
    }

    if (header_end == std::string::npos) {
      header_end = raw.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        HttpRequest partial;
        std::string parse_error;
        if (!parseHttp(raw.substr(0, header_end + 4), partial, parse_error)) {
          error = parse_error;
          return false;
        }

        // 按 Content-Length 预估总包长度，确保读取完整 body。
        const auto body_len = contentLength(partial);
        if (body_len > kMaxBodyBytes) {
          error = "HTTP body too large";
          return false;
        }
        expected_total = header_end + 4 + body_len;
      }
    }

    if (header_end != std::string::npos && raw.size() >= expected_total) {
      break;
    }
  }

  if (expected_total > 0 && raw.size() > expected_total) {
    raw.resize(expected_total);
  }

  if (!parseHttp(raw, out_request, error)) {
    return false;
  }

  return true;
}

}  // namespace lbl
