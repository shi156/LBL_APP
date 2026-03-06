# LBL C++ HTTPS Auth Server

基于 C++20 + CMake 的 Linux 后台服务，支持 Web/Android 通过 HTTPS 访问，首期实现手机号+密码注册登录并接入 MySQL。

## 功能
- `POST /api/v1/auth/register` 注册
- `POST /api/v1/auth/login` 登录
- `GET /api/v1/users/me` 登录态校验
- `GET /health` 健康检查
- TLS(HTTPS) + epoll + 线程池

## 目录
```text
.
├── CMakeLists.txt
├── include/
├── src/
├── config/
├── scripts/
├── cert/
└── third_party/mysql/
```

## 1. 安装依赖（有 sudo 权限时）
```bash
./scripts/install_deps.sh
```

## 2. 无 root 权限时（当前仓库方案）
已将 MySQL 开发包解压到 `third_party/mysql`，可直接编译。

如需重新下载：
```bash
apt download default-libmysqlclient-dev libmysqlclient-dev libmysqlclient21
mkdir -p third_party/mysql
dpkg-deb -x libmysqlclient-dev_*.deb third_party/mysql
dpkg-deb -x libmysqlclient21_*.deb third_party/mysql
```

## 3. 初始化 MySQL
```bash
# 启动 mysql 后执行
mysql -uroot -p < scripts/init_db.sql
```

按需修改配置：
```bash
cp config/app.ini.example config/app.ini
vim config/app.ini
```

## 4. 生成自签名证书（开发环境）
```bash
./scripts/gen_self_signed_cert.sh
```

## 5. 编译
推荐一键编译（会清理交叉编译环境变量污染）：
```bash
./build.sh
```

手动方式：
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

若未安装 Ninja，可改用：
```bash
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## 6. 运行
```bash
./build/lbl_server ./config/app.ini
```

或直接：
```bash
./scripts/run_server.sh
```

## 7. 接口示例
```bash
curl -k https://127.0.0.1:1818/health

curl -k -X POST https://127.0.0.1:1818/api/v1/auth/register \
  -H "Content-Type: application/json" \
  -d '{"phone":"13800138000","password":"Abc12345","confirm_password":"Abc12345"}'

curl -k -X POST https://127.0.0.1:1818/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"phone":"13800138000","password":"Abc12345"}'
```

或直接执行：
```bash
./scripts/test_api.sh
```

如果本机设置了代理（`https_proxy/http_proxy`），请绕过本地地址：
```bash
curl --noproxy '*' -k https://127.0.0.1:1818/health
```

## 8. 数据库查询（用户列表）
查询当前用户列表：
```bash
./scripts/query_users.sh
```

或手动 SQL：
```sql
SELECT id, phone, status, created_at, updated_at
FROM users
ORDER BY id DESC
LIMIT 50;
```

## 9. 日志查看
服务会自动在项目根目录创建 `logs/` 并写入：
- `logs/access.log`：每个请求的 IP、方法、路径、状态码、耗时
- `logs/error.log`：错误日志
- `logs/server.log`：服务运行与错误汇总

常用查看命令：
```bash
tail -f logs/access.log
tail -f logs/error.log
tail -f logs/server.log
```

## 说明
- 密码使用 `PBKDF2-HMAC-SHA256` 存储（含随机盐与迭代次数）。
- Token 为随机高熵字符串并持久化到 `auth_tokens` 表。
- 生产环境请替换为正式证书、限制 CORS、增加限流与审计。
