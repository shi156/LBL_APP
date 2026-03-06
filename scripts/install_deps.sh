#!/usr/bin/env bash
set -euo pipefail

sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  libssl-dev \
  default-libmysqlclient-dev \
  mysql-server
