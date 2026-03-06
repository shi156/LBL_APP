#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <openssl/ssl.h>

#include "config/app_config.h"
#include "core/thread_pool.h"
#include "db/database.h"
#include "http/http_request.h"
#include "security/password_hasher.h"
#include "security/token_service.h"

namespace lbl {

class HttpsServer {
 public:
  // 构造 HTTPS 服务。
  // config: 运行参数；database/password_hasher/token_service: 业务依赖模块。
  HttpsServer(const AppConfig& config,
              Database& database,
              PasswordHasher& password_hasher,
              TokenService& token_service);
  // 析构时释放网络、TLS、线程池资源。
  ~HttpsServer();

  // 初始化 TLS 上下文、线程池、监听 socket。
  bool init(std::string& error);
  // 进入 epoll 事件循环并分发连接到工作线程。
  bool run(std::string& error);
  // 停止服务并释放资源。
  void stop();

 private:
  // 初始化证书、私钥与 TLS 协议版本。
  bool initTls(std::string& error);
  // 创建并绑定监听端口。
  bool initListenSocket(std::string& error);

  // 处理单个客户端连接（TLS 握手、读请求、写响应）。
  // client_ip 用于访问日志记录。
  void handleClient(int client_fd, const std::string& client_ip);
  // 路由分发与业务处理，返回 JSON 字符串。
  std::string dispatchRequest(const HttpRequest& request, int& out_status_code);

  AppConfig config_;
  Database& database_;
  PasswordHasher& password_hasher_;
  TokenService& token_service_;

  std::unique_ptr<ThreadPool> workers_;
  SSL_CTX* ssl_ctx_ = nullptr;
  int listen_fd_ = -1;
  int epoll_fd_ = -1;
  std::atomic<bool> running_{false};
};

}  // namespace lbl
