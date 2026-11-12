#!/usr/bin/env bash
# deploy.sh —— 把编译好的 .ko 传到 guest 的 ~/driver-prep-mods
# 用法：bash scripts/deploy.sh
set -euo pipefail

PROJ_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VM_HOME="${VM_HOME:-$HOME/teco-vm}"
SSH_PORT="${SSH_PORT:-2222}"
KEY="$VM_HOME/id_ed25519"
WEEKS=(week1-hello week2-edu-probe week3-edu-irq week4-edu-dma)

log() { printf '[deploy] %s\n' "$*"; }

SSH_OPTS=(-i "$KEY" -p "$SSH_PORT" -o StrictHostKeyChecking=no \
	  -o UserKnownHostsFile=/dev/null -o ConnectTimeout=3 -o LogLevel=ERROR)
SCP_OPTS=(-i "$KEY" -P "$SSH_PORT" -o StrictHostKeyChecking=no \
	  -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR)

ssh "${SSH_OPTS[@]}" dev@127.0.0.1 'mkdir -p ~/driver-prep-mods' \
	|| { log "连不上 guest，先运行 scripts/start-vm.sh"; exit 1; }

n=0
for w in "${WEEKS[@]}"; do
	for ko in "$PROJ_DIR/$w"/*.ko; do
		[ -s "$ko" ] || continue
		scp -q "${SCP_OPTS[@]}" "$ko" dev@127.0.0.1:~/driver-prep-mods/
		log "已传 $(basename "$ko")"
		n=$((n + 1))
	done
done

if [ "$n" -eq 0 ]; then
	log "没有找到 .ko，先运行 bash scripts/build.sh"
	exit 1
fi
log "完成（$n 个模块）。guest 内：ls ~/driver-prep-mods"
