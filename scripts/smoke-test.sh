#!/usr/bin/env bash
# smoke-test.sh —— 全自动验收：
#   起 VM -> 在 WSL 里编译 4 个模块 -> 传进 guest -> 逐个 insmod 实测 -> 关机
# 用法：bash /mnt/d/2FPGA/driver-prep/scripts/smoke-test.sh
set -uo pipefail

PROJ_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VM_HOME="${VM_HOME:-$HOME/teco-vm}"
SSH_PORT="${SSH_PORT:-2222}"
KEY="$VM_HOME/id_ed25519"

SSH_OPTS=(-i "$KEY" -p "$SSH_PORT" -o StrictHostKeyChecking=no \
	  -o UserKnownHostsFile=/dev/null -o ConnectTimeout=5 -o LogLevel=ERROR)

echo "########## 1/4 启动虚拟机 ##########"
if ! bash "$PROJ_DIR/scripts/start-vm.sh"; then
	echo "VM 启动失败"
	exit 1
fi

echo "########## 2/4 在 WSL 内编译模块 ##########"
if ! bash "$PROJ_DIR/scripts/build.sh"; then
	echo "编译失败"
	bash "$PROJ_DIR/scripts/stop-vm.sh"
	exit 1
fi

echo "########## 3/4 部署到 guest ##########"
if ! bash "$PROJ_DIR/scripts/deploy.sh"; then
	echo "部署失败"
	bash "$PROJ_DIR/scripts/stop-vm.sh"
	exit 1
fi

echo "########## 4/4 guest 内实测 ##########"
ssh "${SSH_OPTS[@]}" dev@127.0.0.1 'bash -s' <<'EOS'
set -u
cd ~/driver-prep-mods || exit 1
rc=0
pass() { echo "  [PASS] $*"; }
fail() { echo "  [FAIL] $*"; rc=1; }

sudo mountpoint -q /sys/kernel/debug || sudo mount -t debugfs none /sys/kernel/debug

echo "== week1: hello 模块 =="
sudo dmesg -C
sudo insmod hello.ko && sleep 0.3
sudo dmesg | grep -q "hello: loaded" && pass "加载 hello.ko" || fail "加载 hello.ko"
sudo rmmod hello

echo "== week2: PCI probe / BAR / 寄存器 =="
sudo dmesg -C
sudo insmod edu_probe.ko && sleep 0.5
sudo dmesg | grep -q "ID=0x010000ed" && pass "读到 edu 卡 ID" || fail "读 ID"
sudo dmesg | grep -q "liveness 通过" && pass "liveness 校验" || fail "liveness"
sudo dmesg | grep -q "factorial 通过" && pass "阶乘自检 5!=120" || fail "阶乘自检"
echo 10 | sudo tee /sys/kernel/debug/edu/factorial >/dev/null
v=$(sudo cat /sys/kernel/debug/edu/factorial)
[ "$v" = "3628800" ] && pass "debugfs 10! = $v" || fail "debugfs 10! = $v"
sudo rmmod edu_probe

echo "== week3: MSI/INTx 中断 =="
sudo dmesg -C
sudo insmod edu_irq.ko && sleep 0.3
echo 8 | sudo tee /sys/kernel/debug/edu/factorial >/dev/null
v=$(sudo cat /sys/kernel/debug/edu/factorial)
[ "$v" = "40320" ] && pass "中断唤醒后 8! = $v" || fail "中断唤醒后 8! = $v"
sudo dmesg | grep -q "次中断" && pass "中断处理函数被调用" || fail "中断处理函数"
sudo rmmod edu_irq

echo "== week4: DMA 往返 =="
sudo dmesg -C
sudo insmod edu_dma.ko && sleep 0.3
echo 1 | sudo tee /sys/kernel/debug/edu/dma >/dev/null 2>&1 \
	&& pass "DMA debugfs 触发" || fail "DMA 触发"
sudo dmesg | grep -q "DMA 往返成功" && pass "DMA 往返数据一致" || fail "DMA 往返"
sudo rmmod edu_dma

echo "== 环境信息 =="
echo "  kernel: $(uname -r)"
echo "  edu 设备: $(lspci -n 2>/dev/null | grep 1234:11e8 || echo '未发现!')"

[ $rc -eq 0 ] && echo "SMOKE-OK" || echo "SMOKE-FAIL"
exit $rc
EOS
rc=$?

echo
echo "########## 关闭虚拟机 ##########"
bash "$PROJ_DIR/scripts/stop-vm.sh"

if [ "$rc" -eq 0 ]; then
	echo "==== 全部通过 ===="
else
	echo "==== 有失败项，看上面 [FAIL] ===="
fi
exit "$rc"
