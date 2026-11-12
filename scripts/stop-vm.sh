#!/usr/bin/env bash
# stop-vm.sh —— 优雅关闭练习虚拟机
set -uo pipefail
VM_HOME="${VM_HOME:-$HOME/teco-vm}"
PIDFILE="$VM_HOME/qemu.pid"
KEY="$VM_HOME/id_ed25519"
SSH_PORT="${SSH_PORT:-2222}"

pid=""
if [ -s "$PIDFILE" ]; then
	pid="$(cat "$PIDFILE" 2>/dev/null || true)"
fi

if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
	ssh -i "$KEY" -p "$SSH_PORT" -o StrictHostKeyChecking=no \
		-o UserKnownHostsFile=/dev/null -o ConnectTimeout=3 -o LogLevel=ERROR \
		dev@127.0.0.1 'sudo poweroff' 2>/dev/null || true
	for _ in $(seq 1 30); do
		kill -0 "$pid" 2>/dev/null || break
		sleep 1
	done
	if kill -0 "$pid" 2>/dev/null; then
		kill "$pid" 2>/dev/null || true
		sleep 2
		kill -9 "$pid" 2>/dev/null || true
	fi
fi
rm -f "$PIDFILE"
echo "[stop-vm] 已停止"
