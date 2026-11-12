#!/usr/bin/env bash
# start-vm.sh —— 启动练习用 Ubuntu 虚拟机（QEMU + KVM），不需要买开发板
#
# 在 WSL 里执行：
#   bash /mnt/d/2FPGA/driver-prep/scripts/start-vm.sh
#
# 首次会下载约 600MB 镜像并初始化 guest（只做一次），之后十几秒就绪：
#   ssh -p 2222 dev@127.0.0.1        # 密码 dev
set -euo pipefail

PROJ_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VM_HOME="${VM_HOME:-$HOME/teco-vm}"
SSH_PORT="${SSH_PORT:-2222}"
MEM_MB="${MEM_MB:-4096}"
CPUS="${CPUS:-4}"
IMG_URL="${IMG_URL:-https://cloud-images.ubuntu.com/releases/24.04/release/ubuntu-24.04-server-cloudimg-amd64.img}"

BASE_IMG="$VM_HOME/base.img"
DISK="$VM_HOME/disk.qcow2"
SEED="$VM_HOME/seed.iso"
KEY="$VM_HOME/id_ed25519"
PIDFILE="$VM_HOME/qemu.pid"
SERIAL="$VM_HOME/serial.log"

log() { printf '[start-vm] %s\n' "$*"; }

SSH_OPTS=(-i "$KEY" -p "$SSH_PORT" -o StrictHostKeyChecking=no \
	  -o UserKnownHostsFile=/dev/null -o ConnectTimeout=3 -o LogLevel=ERROR)
SCP_OPTS=(-i "$KEY" -P "$SSH_PORT" -o StrictHostKeyChecking=no \
	  -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR)

mkdir -p "$VM_HOME"

need_pkgs=0
for c in qemu-system-x86_64 qemu-img cloud-localds ssh scp; do
	command -v "$c" >/dev/null 2>&1 || need_pkgs=1
done
if [ "$need_pkgs" -eq 1 ]; then
	log "安装依赖: qemu-system-x86 qemu-utils cloud-image-utils openssh-client"
	sudo apt-get update -qq
	sudo apt-get install -y -qq qemu-system-x86 qemu-utils cloud-image-utils openssh-client
fi

if [ ! -s "$BASE_IMG" ]; then
	log "下载 Ubuntu 24.04 cloud image（约 600MB，只下载一次）..."
	curl -L --fail --retry 3 -o "$BASE_IMG.part" "$IMG_URL"
	mv "$BASE_IMG.part" "$BASE_IMG"
fi

if [ ! -s "$DISK" ]; then
	log "创建 20GB 磁盘（copy-on-write，不占真实空间）..."
	qemu-img create -q -f qcow2 -F qcow2 -b "$BASE_IMG" "$DISK" 20G
fi

if [ ! -s "$KEY" ]; then
	log "生成 ssh 密钥..."
	ssh-keygen -q -t ed25519 -N '' -f "$KEY"
fi

cat > "$VM_HOME/user-data" <<EOF
#cloud-config
hostname: teco-dev
manage_etc_hosts: true
users:
  - name: dev
    sudo: ALL=(ALL) NOPASSWD:ALL
    shell: /bin/bash
    lock_passwd: false
    ssh_authorized_keys:
      - $(cat "$KEY.pub")
chpasswd:
  expire: false
  list: |
    dev:dev
ssh_pwauth: true
EOF
printf 'instance-id: teco-driver-prep\nlocal-hostname: teco-dev\n' > "$VM_HOME/meta-data"
if [ ! -s "$SEED" ] || [ "$VM_HOME/user-data" -nt "$SEED" ]; then
	log "生成 cloud-init seed..."
	cloud-localds "$SEED" "$VM_HOME/user-data" "$VM_HOME/meta-data"
fi

if [ -s "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
	log "虚拟机已在运行（pid $(cat "$PIDFILE")）"
else
	launch_vm() {
		qemu-system-x86_64 "$@" \
			-name teco-driver-prep \
			-smp "$CPUS" -m "$MEM_MB" \
			-drive file="$DISK",if=virtio,format=qcow2 \
			-drive file="$SEED",if=virtio,format=raw \
			-nic user,model=virtio-net-pci,hostfwd=tcp:127.0.0.1:"$SSH_PORT"-:22 \
			-device edu,dma_mask=0xffffffff \
			-virtfs local,path="$PROJ_DIR",mount_tag=hostshare,security_model=none,multidevs=remap \
			-display none -serial "file:$SERIAL" \
			-pidfile "$PIDFILE" -daemonize
	}

	if [ -c /dev/kvm ] && [ -r /dev/kvm ] && [ -w /dev/kvm ] \
	   && launch_vm -enable-kvm -cpu host 2>"$VM_HOME/qemu-launch.err"; then
		log "已用 KVM 硬件加速启动"
	else
		log "KVM 不可用（宿主未开启 VT-x 虚拟化嵌套），改用 TCG 软件模拟"
		log "  -> 能跑，但虚拟机启动和编译会比真机慢 3~10 倍，属正常现象"
		launch_vm -accel tcg,thread=multi -cpu max 2>"$VM_HOME/qemu-launch.err" || {
			log "QEMU 启动失败："
			cat "$VM_HOME/qemu-launch.err" 2>/dev/null || true
			exit 1
		}
	fi
fi

log "等待 SSH 就绪（首次启动约 1~3 分钟）..."
ready=0
for _ in $(seq 1 300); do
	if ssh "${SSH_OPTS[@]}" -o BatchMode=yes dev@127.0.0.1 true 2>/dev/null; then
		ready=1
		break
	fi
	sleep 2
done
if [ "$ready" -ne 1 ]; then
	log "SSH 超时，串口日志尾部："
	tail -40 "$SERIAL" 2>/dev/null || true
	exit 1
fi

if [ "${GUEST_BUILD:-0}" = "1" ]; then
	log "GUEST_BUILD=1：在 guest 内安装编译环境（可选、较慢，需要 guest 能联网）..."
	scp -q "${SCP_OPTS[@]}" "$PROJ_DIR/scripts/guest-setup.sh" dev@127.0.0.1:/tmp/guest-setup.sh
	ssh "${SSH_OPTS[@]}" 'bash /tmp/guest-setup.sh'
fi

cat <<EOF

 环境就绪
   登录:      ssh -p $SSH_PORT dev@127.0.0.1      （密码 dev，也支持 key）
   VSCode:    Remote-SSH 连接 127.0.0.1:$SSH_PORT，用户名 dev
   编译:      bash $PROJ_DIR/scripts/build.sh      （在 WSL 里编译，快）
   部署:      bash $PROJ_DIR/scripts/deploy.sh     （把 .ko 传进 guest）
   练习:      guest 内 cd ~/driver-prep-mods
   全自动验收: bash $PROJ_DIR/scripts/smoke-test.sh
   串口日志:  tail -f $SERIAL
   停止:      bash $PROJ_DIR/scripts/stop-vm.sh

EOF
