#!/usr/bin/env bash
# guest-setup.sh —— 在虚拟机内部执行：装编译环境、挂 9p 共享、同步工程
# 由 start-vm.sh 自动调用，一般不需要手动运行。
# 幂等：可以反复执行；只有编译环境真正装好才会写 ~/.teco-ready 标记。
set -uo pipefail
export DEBIAN_FRONTEND=noninteractive

# 云镜像自带的自动更新会和我们的 apt 抢网络连接和 apt 锁，先停掉（只在可选流程里用到）
sudo systemctl stop unattended-upgrades.service apt-daily.service apt-daily-upgrade.service 2>/dev/null || true
sudo systemctl disable unattended-upgrades.service apt-daily.timer apt-daily-upgrade.timer 2>/dev/null || true

APT_OPTS=(-y -qq
	-o Acquire::Retries=6
	-o Acquire::http::Timeout=30
	-o Acquire::https::Timeout=30
	-o Dpkg::Options::=--force-confold)

echo "[guest] apt update..."
sudo apt-get update -qq -o Acquire::Retries=6 || echo "[guest] apt update 失败，继续"

echo "[guest] 安装编译工具 + 内核头文件（首次约 5~10 分钟，TCG 模拟下偏慢）..."
sudo apt-get install "${APT_OPTS[@]}" \
	build-essential "linux-headers-$(uname -r)" kmod pciutils rsync vim git \
	|| echo "[guest] 部分包安装失败，继续"

echo "[guest] 确保 debugfs 已挂载..."
mountpoint -q /sys/kernel/debug || sudo mount -t debugfs none /sys/kernel/debug

echo "[guest] 尝试启用 9p 共享（可选，失败不影响编译）..."
if ! sudo modprobe 9pnet_virtio 2>/dev/null; then
	sudo apt-get install "${APT_OPTS[@]}" "linux-modules-extra-$(uname -r)" 2>/dev/null \
		|| sudo apt-get install "${APT_OPTS[@]}" linux-modules-extra-virtual 2>/dev/null \
		|| true
	sudo modprobe 9pnet_virtio 2>/dev/null || true
fi
sudo mkdir -p /mnt/host
if ! mountpoint -q /mnt/host; then
	sudo mount -t 9p -o trans=virtio,version=9p2000.L hostshare /mnt/host 2>/dev/null \
		|| echo "[guest] 9p 不可用（改用 scp 或 VSCode Remote-SSH 同步，不影响编译）"
fi

mkdir -p ~/driver-prep
if mountpoint -q /mnt/host; then
	rsync -a /mnt/host/ ~/driver-prep/ 2>/dev/null || true
fi

# 只有编译环境真正可用才打标记，避免"半装好"被跳过
if command -v gcc >/dev/null 2>&1 && [ -d "/lib/modules/$(uname -r)/build" ]; then
	touch ~/.teco-ready
	echo "[guest] 完成：kernel=$(uname -r)，编译环境就绪，项目在 ~/driver-prep"
else
	echo "[guest] 警告：编译环境不完整（gcc 或内核头文件缺失），下次启动会重试。"
	echo "        可手动执行：sudo apt-get install build-essential linux-headers-\$(uname -r)"
fi
