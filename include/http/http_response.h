#pragma once

#include <string>

namespace lbl {

// 构造完整 HTTP 响应报文（状态行 + 头 + JSON body）。
// status_code: HTTP 状态码；body_json: JSON 字符串。
std::string buildHttpResponse(int status_code, const std::string& body_json);
// 将状态码映射为 HTTP 状态文本，如 200 -> OK。
std::string statusText(int status_code);

}  // namespace lbl
