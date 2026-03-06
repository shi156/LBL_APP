#include "http/http_response.h"

#include <string>

namespace lbl {

// 状态码到文本映射。
std::string statusText(int status_code) {
  switch (status_code) {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 404:
      return "Not Found";
    case 409:
      return "Conflict";
    case 500:
      return "Internal Server Error";
    default:
      return "OK";
  }
}

// 组装完整 HTTP 响应报文。
std::string buildHttpResponse(int status_code, const std::string& body_json) {
  const auto text = statusText(status_code);
  std::string response;
  response.reserve(256 + body_json.size());

  response += "HTTP/1.1 " + std::to_string(status_code) + " " + text + "\r\n";
  response += "Content-Type: application/json; charset=utf-8\r\n";
  response += "Connection: close\r\n";
  response += "Content-Length: " + std::to_string(body_json.size()) + "\r\n";
  response += "\r\n";
  response += body_json;

  return response;
}

}  // namespace lbl
