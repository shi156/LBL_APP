#pragma once

#include <cstdint>
#include <string>

namespace lbl {

// 服务监听参数：端口、线程数、epoll 参数等。
struct ServerConfig {
  std::string host = "0.0.0.0";
  std::uint16_t port = 1818;
  int worker_threads = 4;
  int epoll_max_events = 128;
  int socket_backlog = 256;
};

// TLS 证书与私钥路径。
struct TlsConfig {
  std::string cert_file = "./cert/server.crt";
  std::string key_file = "./cert/server.key";
};

// MySQL 连接参数。
struct DatabaseConfig {
  std::string host = "127.0.0.1";
  std::uint16_t port = 3306;
  std::string user = "app_user";
  std::string password = "app_pass";
  std::string database = "app_db";
  std::string charset = "utf8mb4";
};

// 认证参数：token 有效期、密码哈希迭代次数。
struct AuthConfig {
  int token_expire_seconds = 7200;
  int pbkdf2_iterations = 120000;
};

// 顶层应用配置对象。
struct AppConfig {
  ServerConfig server;
  TlsConfig tls;
  DatabaseConfig database;
  AuthConfig auth;
};

// 从 key=value 格式配置文件读取配置。
// path: 配置文件路径；config: 输出配置对象；error: 失败错误信息。
bool loadAppConfig(const std::string& path, AppConfig& config, std::string& error);

}  // namespace lbl
