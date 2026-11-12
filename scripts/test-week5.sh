#!/usr/bin/env bash
# 临时脚本：端到端验证 week5 字符设备驱动（WSL -> VM）
set -uo pipefail
PROJ=/mnt/d/2FPGA/driver-prep
VM_HOME="$HOME/teco-vm"
SSH_OPTS=(-i "$VM_HOME/id_ed25519" -p 2222 -o StrictHostKeyChecking=no \
          -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR)
SCP_OPTS=(-i "$VM_HOME/id_ed25519" -P 2222 -o StrictHostKeyChecking=no \
          -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR)

cd "$PROJ" || exit 1

echo "########## 1) 启动 VM ##########"
bash scripts/start-vm.sh >/tmp/startvm.log 2>&1 || { echo "VM 启动失败"; tail -20 /tmp/startvm.log; exit 1; }

echo "########## 2) 编译并部署 ##########"
bash scripts/build.sh week5-chardev-uapi 2>&1 | tail -3
bash scripts/deploy.sh 2>&1 | tail -3

echo "########## 3) 编译用户态测试程序（静态链接，兼容 guest glibc） ##########"
gcc -static -Wall -Wextra -O2 -Iweek5-chardev-uapi -o /tmp/test_edu5 week5-chardev-uapi/app/test_edu5.c || { echo "app 编译失败"; exit 1; }
scp -q "${SCP_OPTS[@]}" /tmp/test_edu5 dev@127.0.0.1:/tmp/test_edu5 || { echo "scp 失败"; exit 1; }

echo "########## 4) guest 内加载 + 测试 ##########"
ssh "${SSH_OPTS[@]}" dev@127.0.0.1 'bash -s' <<'EOS'
set -u
sudo dmesg -C
echo "--- insmod ---"
sudo insmod ~/driver-prep-mods/edu_chardev.ko
sleep 0.5
echo "--- /dev/edu5 ---"
ls -l /dev/edu5 2>&1
echo "--- dmesg ---"
sudo dmesg | tail -4
echo "--- 用户态测试 ---"
sudo /tmp/test_edu5
rc=$?
echo "--- dmesg after ---"
sudo dmesg | tail -6
sudo rmmod edu_chardev 2>&1
echo "--- after rmmod ---"
ls /dev/edu5 2>&1 || echo "/dev/edu5 已移除"
exit $rc
EOS
rc=$?

echo "########## 5) 关闭 VM ##########"
bash scripts/stop-vm.sh
if [ $rc -eq 0 ]; then echo "==== WEEK5-E2E-OK ===="; else echo "==== WEEK5-E2E-FAIL ===="; fi
exit $rc
