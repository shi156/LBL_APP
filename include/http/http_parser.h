#pragma once

#include <openssl/ssl.h>

#include <string>

#include "http/http_request.h"

namespace lbl {

// 从 TLS 连接读取并解析一条 HTTP 请求。
// ssl: TLS 会话；out_request: 输出解析结果；error: 失败错误信息。
bool readHttpRequest(SSL* ssl, HttpRequest& out_request, std::string& error);

}  // namespace lbl
