#!/bin/bash
set -euo pipefail

echo "Запуск скрипта сборки..."

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ ! -d "third_party" ]; then
  echo "Ошибка: директория third_party не найдена!" >&2
  exit 1
fi
cd third_party

echo "Установка зависимостей..."
sudo apt update
sudo apt install -y python3-pip python3-setuptools cmake g++ libpython3-dev \
    python3-numpy swig python3-matplotlib libxml2 libxml2-dev bison flex \
    libcdk5-dev libusb-1.0-0-dev libaio-dev pkg-config \
    libavahi-common-dev libavahi-client-dev

build_project() {
  local name=$1
  local repo=$2
  local branch=$3
  
  echo "🔧 Сборка $name..."
  if [ ! -d "$name" ]; then
    git clone --branch "$branch" "$repo"
  else
    echo "$name уже существует, пропускаем клонирование"
  fi
  
  cd "$name"
  rm -rf build && mkdir build && cd build
  cmake ../
  make -j"$(nproc)"
  sudo make install
  sudo ldconfig
  cd "$SCRIPT_DIR/third_party"
}

build_project "SoapySDR" "https://github.com/TelecomDep/SoapySDR.git" "soapy-sdr-0.8.1"
build_project "libiio" "https://github.com/TelecomDep/libiio.git" "v0.24"
build_project "SoapyPlutoSDR" "https://github.com/TelecomDep/SoapyPlutoSDR.git" "sdr_gadget_timestamping"

echo "Сборка завершена!"