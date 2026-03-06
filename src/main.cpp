#include <signal.h>

#include <chrono>
#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "config/app_config.h"
#include "db/database.h"
#include "security/password_hasher.h"
#include "security/token_service.h"
#include "server/https_server.h"
#include "utils/logger.h"

namespace {

volatile sig_atomic_t g_stop_requested = 0;

// 信号处理函数：仅设置停止标记，避免在信号上下文做复杂逻辑。
void onSignal(int) {
  g_stop_requested = 1;
}

}  // namespace

int main(int argc, char* argv[]) {
  // 支持通过命令行传入配置路径，默认使用 ./config/app.ini。
  const std::string config_path = argc > 1 ? argv[1] : "./config/app.ini";

  lbl::AppConfig config;
  std::string error;
  if (!lbl::loadAppConfig(config_path, config, error)) {
    std::cerr << "Load config failed: " << error << std::endl;
    lbl::logger::error("Load config failed: " + error);
    return 1;
  }

  lbl::Database db;
  if (!db.connect(config.database, error)) {
    std::cerr << "Database connect failed: " << error << std::endl;
    lbl::logger::error("Database connect failed: " + error);
    return 1;
  }
  if (!db.initSchema(error)) {
    std::cerr << "Init schema failed: " << error << std::endl;
    lbl::logger::error("Init schema failed: " + error);
    return 1;
  }

  lbl::PasswordHasher password_hasher(config.auth.pbkdf2_iterations);
  lbl::TokenService token_service;

  auto server = std::make_unique<lbl::HttpsServer>(config, db, password_hasher, token_service);
  if (!server->init(error)) {
    std::cerr << "Server init failed: " << error << std::endl;
    lbl::logger::error("Server init failed: " + error);
    return 1;
  }

  signal(SIGINT, onSignal);
  signal(SIGTERM, onSignal);

  // 在独立线程运行服务主循环，主线程用于信号监听与优雅退出。
  std::string run_error;
  std::atomic<bool> run_ok{true};
  std::thread server_thread([&server, &run_error, &run_ok]() {
    if (!server->run(run_error)) {
      run_ok.store(false);
    }
  });

  // Keep main thread alive for signal handling and graceful shutdown.
  while (g_stop_requested == 0 && run_ok.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  server->stop();
  if (server_thread.joinable()) {
    server_thread.join();
  }

  if (!run_ok.load()) {
    std::cerr << "Server run failed: " << run_error << std::endl;
    lbl::logger::error("Server run failed: " + run_error);
    return 1;
  }

  lbl::logger::info("Server stopped");
  return 0;
}
