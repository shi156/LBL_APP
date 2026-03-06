#include "server/https_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cctype>
#include <cstring>
#include <regex>
#include <string>
#include <vector>

#include "http/http_parser.h"
#include "http/http_response.h"
#include "utils/logger.h"
#include "utils/string_utils.h"

namespace lbl {
namespace {

// 将 fd 设置为非阻塞，配合 epoll 使用。
int setNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return -1;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 手机号校验（中国大陆 11 位）。
bool isValidPhone(const std::string& phone) {
  static const std::regex pattern("^1[3-9]\\d{9}$");
  return std::regex_match(phone, pattern);
}

// 密码校验：长度 8~64，且至少包含字母和数字。
bool isValidPassword(const std::string& password) {
  if (password.size() < 8 || password.size() > 64) {
    return false;
  }

  bool has_alpha = false;
  bool has_digit = false;
  for (const auto c : password) {
    if (std::isalpha(static_cast<unsigned char>(c))) {
      has_alpha = true;
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
      has_digit = true;
    }
  }
  return has_alpha && has_digit;
}

// 业务成功统一响应体。
std::string okBody(const std::string& data_json) {
  return std::string("{\"code\":0,\"message\":\"ok\",\"data\":") + data_json + "}";
}

// 业务失败统一响应体。
std::string errorBody(int code, const std::string& message) {
  return "{\"code\":" + std::to_string(code) + ",\"message\":\"" +
         utils::jsonEscape(message) + "\",\"data\":{}}";
}

// 返回 method 的小写副本，便于路由比较。
std::string toLowerCopy(const std::string& value) {
  return utils::toLower(value);
}

}  // namespace

// 构造函数：保存配置和依赖对象引用。
HttpsServer::HttpsServer(const AppConfig& config,
                         Database& database,
                         PasswordHasher& password_hasher,
                         TokenService& token_service)
    : config_(config),
      database_(database),
      password_hasher_(password_hasher),
      token_service_(token_service) {}

// 析构函数：确保服务停止并释放资源。
HttpsServer::~HttpsServer() {
  stop();
}

// 初始化服务所需资源。
bool HttpsServer::init(std::string& error) {
  // worker_threads 由配置控制，可按 CPU 核心调整。
  workers_ = std::make_unique<ThreadPool>(config_.server.worker_threads);
  if (!initTls(error)) {
    return false;
  }
  if (!initListenSocket(error)) {
    return false;
  }
  return true;
}

// 主事件循环：accept 新连接并派发给线程池。
bool HttpsServer::run(std::string& error) {
  if (listen_fd_ < 0 || ssl_ctx_ == nullptr || workers_ == nullptr) {
    error = "Server not initialized";
    return false;
  }

  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd_ < 0) {
    error = std::string("epoll_create1 failed: ") + std::strerror(errno);
    logger::error(error);
    return false;
  }

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = listen_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
    error = std::string("epoll_ctl ADD listen_fd failed: ") + std::strerror(errno);
    logger::error(error);
    return false;
  }

  running_.store(true);
  logger::info("Server started on https://" + config_.server.host + ":" +
               std::to_string(config_.server.port) + " with " +
               std::to_string(config_.server.worker_threads) + " workers");

  // epoll_max_events 是单次事件批量处理上限，可按并发量调大。
  std::vector<epoll_event> events(static_cast<std::size_t>(config_.server.epoll_max_events));

  while (running_.load()) {
    const int n = epoll_wait(epoll_fd_, events.data(), static_cast<int>(events.size()), 1000);

    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      error = std::string("epoll_wait failed: ") + std::strerror(errno);
      logger::error(error);
      return false;
    }

    for (int i = 0; i < n; ++i) {
      if (events[static_cast<std::size_t>(i)].data.fd != listen_fd_) {
        continue;
      }

      while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const int client_fd = accept4(listen_fd_,
                                      reinterpret_cast<sockaddr*>(&client_addr),
                                      &client_len,
                                      SOCK_CLOEXEC);
        if (client_fd < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
          }
          logger::error(std::string("accept4 failed: ") + std::strerror(errno));
          break;
        }

        char ip_buf[INET_ADDRSTRLEN] = {0};
        std::string client_ip = "unknown";
        if (inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf)) != nullptr) {
          client_ip = ip_buf;
        }

        workers_->enqueue([this, client_fd, client_ip]() { handleClient(client_fd, client_ip); });
      }
    }
  }

  return true;
}

// 主动停止服务并释放资源。
void HttpsServer::stop() {
  running_.store(false);

  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
    epoll_fd_ = -1;
  }

  if (listen_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
  }

  if (workers_ != nullptr) {
    workers_->stop();
    workers_.reset();
  }

  if (ssl_ctx_ != nullptr) {
    SSL_CTX_free(ssl_ctx_);
    ssl_ctx_ = nullptr;
  }
}

// 初始化 TLS：加载证书和私钥并校验。
bool HttpsServer::initTls(std::string& error) {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();

  ssl_ctx_ = SSL_CTX_new(TLS_server_method());
  if (ssl_ctx_ == nullptr) {
    error = "SSL_CTX_new failed";
    logger::error(error);
    return false;
  }

  // 强制最低 TLS1.2，避免过旧协议。
  SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_2_VERSION);

  if (SSL_CTX_use_certificate_file(ssl_ctx_, config_.tls.cert_file.c_str(), SSL_FILETYPE_PEM) <=
      0) {
    error = "Failed to load cert file: " + config_.tls.cert_file;
    logger::error(error);
    return false;
  }

  if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, config_.tls.key_file.c_str(), SSL_FILETYPE_PEM) <=
      0) {
    error = "Failed to load key file: " + config_.tls.key_file;
    logger::error(error);
    return false;
  }

  if (SSL_CTX_check_private_key(ssl_ctx_) != 1) {
    error = "TLS private key does not match certificate";
    logger::error(error);
    return false;
  }

  return true;
}

// 初始化监听 socket 并绑定到配置端口。
bool HttpsServer::initListenSocket(std::string& error) {
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    error = std::string("socket failed: ") + std::strerror(errno);
    logger::error(error);
    return false;
  }

  int opt = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(config_.server.port);

  if (inet_pton(AF_INET, config_.server.host.c_str(), &addr.sin_addr) != 1) {
    error = "Invalid server.host: " + config_.server.host;
    logger::error(error);
    return false;
  }

  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    error = std::string("bind failed: ") + std::strerror(errno);
    logger::error(error);
    return false;
  }

  // socket_backlog 可按高并发场景调大。
  if (listen(listen_fd_, config_.server.socket_backlog) < 0) {
    error = std::string("listen failed: ") + std::strerror(errno);
    logger::error(error);
    return false;
  }

  if (setNonBlocking(listen_fd_) < 0) {
    error = std::string("setNonBlocking failed: ") + std::strerror(errno);
    logger::error(error);
    return false;
  }

  return true;
}

// 处理单个客户端会话。
void HttpsServer::handleClient(int client_fd, const std::string& client_ip) {
  const auto start = std::chrono::steady_clock::now();

  std::string method_for_log = "-";
  std::string path_for_log = "-";
  std::string note;

  SSL* ssl = SSL_new(ssl_ctx_);
  if (ssl == nullptr) {
    logger::error("SSL_new failed");
    close(client_fd);
    return;
  }

  SSL_set_fd(ssl, client_fd);
  if (SSL_accept(ssl) <= 0) {
    note = "tls_handshake_failed";
    logger::access(client_ip, method_for_log, path_for_log, 400, 0, note);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_fd);
    return;
  }

  HttpRequest request;
  std::string error;
  int status_code = 500;
  std::string body;

  if (!readHttpRequest(ssl, request, error)) {
    status_code = 400;
    body = errorBody(1001, "bad request");
    note = error;
  } else {
    method_for_log = request.method;
    path_for_log = request.path;
    body = dispatchRequest(request, status_code);
  }

  const auto response = buildHttpResponse(status_code, body);
  std::size_t sent = 0;
  while (sent < response.size()) {
    const int n = SSL_write(ssl, response.data() + sent, static_cast<int>(response.size() - sent));
    if (n <= 0) {
      note = note.empty() ? "ssl_write_failed" : note + ";ssl_write_failed";
      break;
    }
    sent += static_cast<std::size_t>(n);
  }

  SSL_shutdown(ssl);
  SSL_free(ssl);
  close(client_fd);

  const auto end = std::chrono::steady_clock::now();
  const auto cost_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  logger::access(client_ip, method_for_log, path_for_log, status_code, cost_ms, note);
}

// 路由分发 + 业务处理。
std::string HttpsServer::dispatchRequest(const HttpRequest& request, int& out_status_code) {
  const auto method = toLowerCopy(request.method);

  if (method == "get" && request.path == "/health") {
    out_status_code = 200;
    return okBody("{\"status\":\"up\"}");
  }

  if (method == "post" && request.path == "/api/v1/auth/register") {
    const auto phone = utils::extractJsonString(request.body, "phone");
    const auto password = utils::extractJsonString(request.body, "password");
    const auto confirm = utils::extractJsonString(request.body, "confirm_password");

    if (!phone.has_value() || !password.has_value() || !confirm.has_value()) {
      out_status_code = 400;
      return errorBody(1002, "phone/password/confirm_password required");
    }
    if (!isValidPhone(*phone)) {
      out_status_code = 400;
      return errorBody(1003, "invalid phone format");
    }
    if (*password != *confirm) {
      out_status_code = 400;
      return errorBody(1004, "password and confirm_password not match");
    }
    if (!isValidPassword(*password)) {
      out_status_code = 400;
      return errorBody(1005, "password must be 8-64 chars and contain letters and digits");
    }

    std::string password_record;
    if (!password_hasher_.createPasswordRecord(*password, password_record)) {
      out_status_code = 500;
      return errorBody(1500, "failed to hash password");
    }

    std::uint64_t user_id = 0;
    std::string db_error;
    if (!database_.createUser(*phone, password_record, user_id, db_error)) {
      if (db_error == "phone already registered") {
        out_status_code = 409;
        return errorBody(1006, db_error);
      }
      out_status_code = 500;
      return errorBody(1501, db_error);
    }

    out_status_code = 201;
    return okBody("{\"user_id\":" + std::to_string(user_id) + "}");
  }

  if (method == "post" && request.path == "/api/v1/auth/login") {
    const auto phone = utils::extractJsonString(request.body, "phone");
    const auto password = utils::extractJsonString(request.body, "password");

    if (!phone.has_value() || !password.has_value()) {
      out_status_code = 400;
      return errorBody(1007, "phone/password required");
    }

    std::string db_error;
    const auto user = database_.findUserByPhone(*phone, db_error);
    if (!db_error.empty()) {
      out_status_code = 500;
      return errorBody(1502, db_error);
    }
    if (!user.has_value() || user->status != 1) {
      out_status_code = 401;
      return errorBody(1101, "invalid phone or password");
    }
    if (!password_hasher_.verifyPassword(*password, user->password_hash)) {
      out_status_code = 401;
      return errorBody(1101, "invalid phone or password");
    }

    std::string token;
    if (!token_service_.generateToken(token)) {
      out_status_code = 500;
      return errorBody(1503, "failed to generate token");
    }

    // token_expire_seconds 由配置控制，修改后会直接影响签发有效期。
    if (!database_.saveToken(user->id, token, config_.auth.token_expire_seconds, db_error)) {
      out_status_code = 500;
      return errorBody(1504, db_error);
    }

    out_status_code = 200;
    return okBody("{\"access_token\":\"" + token +
                  "\",\"token_type\":\"Bearer\",\"expires_in\":" +
                  std::to_string(config_.auth.token_expire_seconds) + "}");
  }

  if (method == "get" && request.path == "/api/v1/users/me") {
    auto it = request.headers.find("authorization");
    if (it == request.headers.end()) {
      out_status_code = 401;
      return errorBody(1102, "missing authorization header");
    }

    const std::string bearer = "Bearer ";
    const auto& auth_value = it->second;
    if (!utils::startsWith(auth_value, bearer)) {
      out_status_code = 401;
      return errorBody(1103, "invalid authorization header");
    }

    const auto token = auth_value.substr(bearer.size());
    std::string db_error;
    const auto user = database_.findUserByToken(token, db_error);
    if (!db_error.empty()) {
      out_status_code = 500;
      return errorBody(1505, db_error);
    }
    if (!user.has_value() || user->status != 1) {
      out_status_code = 401;
      return errorBody(1104, "token expired or invalid");
    }

    out_status_code = 200;
    return okBody("{\"id\":" + std::to_string(user->id) + ",\"phone\":\"" +
                  utils::jsonEscape(user->phone) + "\"}");
  }

  out_status_code = 404;
  return errorBody(1404, "route not found");
}

}  // namespace lbl

