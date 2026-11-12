#!/usr/bin/env bash
# build.sh —— 在 WSL 里为 guest 内核编译模块（推荐方式）
#
# 为什么不在 guest 里直接 make：
#   - 本机 BIOS 未开 VT-x，虚拟机只能 TCG 软件模拟，guest 内编译慢 3~10 倍
#   - guest 的 apt 不稳定（云镜像自带 unattended-upgrades 抢占连接）
# 所以：WSL 里用与 guest 完全一致的内核头文件编译，再把 .ko 传进 guest。
# 编译出的模块 vermagic 与 guest 内核一致，insmod 能正常加载。
#
# 用法：
#   bash scripts/build.sh                    # 编译全部 4 周
#   bash scripts/build.sh week2-edu-probe    # 只编译某个
#   KVER=6.8.0-147-generic bash scripts/build.sh   # 手动指定内核版本
set -euo pipefail

PROJ_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VM_HOME="${VM_HOME:-$HOME/teco-vm}"
SSH_PORT="${SSH_PORT:-2222}"
KEY="$VM_HOME/id_ed25519"
KH_ROOT="$VM_HOME/kheaders"
BASE="http://archive.ubuntu.com/ubuntu"
DEFAULT_KVER="6.8.0-147-generic"
WEEKS=(week1-hello week2-edu-probe week3-edu-irq week4-edu-dma)

log() { printf '[build] %s\n' "$*"; }

SSH_OPTS=(-i "$KEY" -p "$SSH_PORT" -o StrictHostKeyChecking=no \
	  -o UserKnownHostsFile=/dev/null -o ConnectTimeout=3 -o LogLevel=ERROR)

mkdir -p "$VM_HOME"

# ---------- 1) 确定 guest 内核版本 ----------
KVER="${KVER:-}"
if [ -z "$KVER" ]; then
	if ssh "${SSH_OPTS[@]}" -o BatchMode=yes dev@127.0.0.1 uname -r \
		2>/dev/null >"$VM_HOME/kver.tmp"; then
		KVER="$(cat "$VM_HOME/kver.tmp")"
	else
		KVER="$(cat "$VM_HOME/kver" 2>/dev/null || true)"
	fi
fi
[ -n "$KVER" ] || KVER="$DEFAULT_KVER"
echo "$KVER" > "$VM_HOME/kver"
KABI="$KVER"
KABI="${KABI%-generic}"
log "guest kernel = $KVER"

# ---------- 2) 准备与 guest 一致的内核头文件 ----------
HDR_DIR="$KH_ROOT/root/usr/src/linux-headers-$KVER"
if [ ! -d "$HDR_DIR" ]; then
	log "首次运行：下载 $KVER 内核头文件（约 20MB，只下一次）..."
	mkdir -p "$KH_ROOT"
	for pkg in "linux-headers-$KABI" "linux-headers-$KVER"; do
		file=""
		for dist in noble-updates noble-security noble; do
			cache="$KH_ROOT/Packages-$dist.gz"
			if [ ! -s "$cache" ]; then
				curl -s -m 120 "$BASE/dists/$dist/main/binary-amd64/Packages.gz" \
					-o "$cache" || true
			fi
			file="$(zcat "$cache" 2>/dev/null | awk -v p="$pkg" '
				$1 == "Package:" { want = ($2 == p) }
				want && $1 == "Filename:" { print $2; exit }')"
			[ -n "$file" ] && break
		done
		[ -n "$file" ] || { log "找不到 $pkg 的下载地址"; exit 1; }
		deb="$KH_ROOT/$(basename "$file")"
		[ -s "$deb" ] || curl -sL -m 300 "$BASE/$file" -o "$deb"
		dpkg-deb -x "$deb" "$KH_ROOT/root"
	done
fi
[ -d "$HDR_DIR" ] || { log "头文件解压失败：$HDR_DIR 不存在"; exit 1; }
log "headers = $HDR_DIR"

# ---------- 3) 编译器（Ubuntu 内核头文件里写死用 gcc-13） ----------
if ! command -v gcc-13 >/dev/null 2>&1; then
	log "安装 gcc-13（Ubuntu 内核头文件指定版本）..."
	sudo apt-get update -qq
	sudo apt-get install -y -qq gcc-13
fi

# ---------- 4) 编译 ----------
weeks=("$@")
if [ "${#weeks[@]}" -eq 0 ]; then
	weeks=("${WEEKS[@]}")
fi
for w in "${weeks[@]}"; do
	dir="$PROJ_DIR/$w"
	[ -d "$dir" ] || { log "目录不存在：$dir"; exit 1; }
	log "编译 $w ..."
	if ! make -C "$HDR_DIR" M="$dir" modules >"/tmp/$w.build.log" 2>&1; then
		log "编译失败，最后 25 行日志："
		tail -25 "/tmp/$w.build.log"
		exit 1
	fi
	ko="$(ls "$dir"/*.ko 2>/dev/null | head -1 || true)"
	[ -n "$ko" ] || { log "没有生成 .ko"; exit 1; }
	log "OK: $(basename "$ko")  (vermagic 应与 guest 一致)"
done
log "完成。下一步：bash scripts/deploy.sh 把模块传进虚拟机"
