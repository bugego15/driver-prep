# 太初元碁·驱动/内核方向 · 4 周动手练习环境

> 目标岗位：太初元碁（Tecorigin）「驱动研发工程师（内存/调度）」「工具链研发工程师」「AI 软件架构」这类基础软件岗。
> 核心逻辑：**不用买开发板**。用 QEMU 里的一块虚拟 PCIe 卡（`edu` 教学设备），把「PCI 驱动 / MMIO / 中断 / DMA」这四件事亲手写一遍 —— 这正是 JD 里"加分项"最看重的项目经历。

---

## 0. 这台机器的实际情况（已探测）

| 项 | 状态 |
|---|---|
| WSL2 | Ubuntu 26.04 LTS，默认用户 root，免密 sudo |
| CPU / 内存 | Intel i7-8700（12 线程）/ 15 GB |
| QEMU | 已安装，带 `edu` 教学设备 |
| KVM | **不可用**（BIOS 未开启 VT-x），自动降级为 TCG 软件模拟 |
| 虚拟机 | Ubuntu 24.04 cloud image，内核 `6.8.0-147-generic`，SSH 转发到 `127.0.0.1:2222` |
| 编译方式 | 在 **WSL 里**用与 guest 完全一致的内核头文件编译，再把 `.ko` 传进虚拟机 |

> **为什么这样设计**：TCG 模拟下 guest 内编译慢 3~10 倍，而且云镜像的 apt 网络不稳定。
> 所以把「编译」放在 WSL 原生执行（快、稳定），虚拟机只负责「加载模块 + 跑测试」——
> 这也是真实工作中的常见做法：**构建环境和运行环境本来就可以分离**。
> 模块的 `vermagic` 与 guest 内核严格一致，`insmod` 可以直接加载。

---

## 1. 快速开始

在 **WSL 终端**里执行（全部脚本都在 `scripts/`）：

```bash
cd /mnt/d/2FPGA/driver-prep

bash scripts/start-vm.sh      # 1) 启动虚拟机（首次约 1~3 分钟）
bash scripts/build.sh         # 2) 编译 4 周模块（首次自动下载内核头文件 ~20MB）
bash scripts/deploy.sh        # 3) 把 .ko 传进虚拟机

ssh -p 2222 dev@127.0.0.1     # 4) 登录（密码 dev）
```

登录后加载第 1 周的模块，看到内核日志：

```bash
cd ~/driver-prep-mods
sudo insmod hello.ko && sudo dmesg | tail -3
sudo rmmod hello
```

**一条命令全自动验收**（起 VM → 编译 → 部署 → 4 个模块逐个实测 → 关机）：

```bash
bash scripts/smoke-test.sh
```

**推荐编辑器**：Windows 上装 VSCode + Remote-SSH，连 `127.0.0.1:2222`（用户 `dev`），
可以直接编辑 guest 里的 `~/driver-prep-mods`；也可以在 WSL 里 `code .` 编辑源码。

---

## 2. 目录结构与 4 周任务

```
driver-prep/
├── week1-hello/          # 最小内核模块：insmod / dmesg / rmmod
├── week2-edu-probe/      # PCI probe + BAR 映射 + MMIO 寄存器 + debugfs
├── week3-edu-irq/        # MSI/INTx 中断 + completion + 中断应答
├── week4-edu-dma/        # 一致性 DMA + 传输 + 数据校验
├── scripts/
│   ├── start-vm.sh       # 起虚拟机（幂等，可反复执行）
│   ├── stop-vm.sh        # 优雅关机
│   ├── build.sh          # WSL 里编译模块（首次自动准备内核头文件 + gcc-13）
│   ├── deploy.sh         # 把 .ko 传到 guest 的 ~/driver-prep-mods
│   ├── smoke-test.sh     # 全自动验收
│   └── guest-setup.sh    # 可选：在 guest 内装编译环境（GUEST_BUILD=1 时才用）
└── docs/concepts-checklist.md   # 每周自测题 + 面试题清单
```

| 周 | 你要做的事 | 验收标准 | 对应岗位技能 |
|---|---|---|---|
| 1 | 读懂 hello 模块，改代码、加日志、`rmmod` 看 `dmesg` | `dmesg` 出现你的日志，能解释 `module_init` 到 `init` 的调用链 | 内核模块基础 |
| 2 | 读 `edu_probe.c`，改阶乘参数、加一个 debugfs 文件读 ID | `5! = 120`、`10! = 3628800`；能解释 BAR 是什么、为什么用 `ioread32` | PCI 驱动 / MMIO / debugfs |
| 3 | 把轮询改成中断；用 `cat /proc/interrupts` 找你的 IRQ | 写 `8` 得到 `40320`，`/proc/interrupts` 中计数增长 | 中断处理 / 并发 |
| 4 | 读懂 DMA 寄存器与往返校验；把 `EDU_DMA_LEN` 改成 4096 | `DMA 往返成功`；能解释 `dma_alloc_coherent` 与 `virt_to_phys` 的区别 | DMA / 内存管理 |

> 每周末尾的"改造任务"写在对应 `.c` 文件开头的注释里。**只看不写等于没学**。

---

## 3. edu 虚拟卡寄存器速查（写代码时对照）

来源：QEMU 官方规范 <https://www.qemu.org/docs/master/specs/edu.html>，PCI ID `1234:11e8`，BAR0 = 1MB MMIO。

| 偏移 | 属性 | 含义 |
|---|---|---|
| 0x00 | RO | ID，值为 `0x010000ed`（主版本 01 / 次版本 00） |
| 0x04 | RW | liveness：读回值是写入值的按位取反 |
| 0x08 | RW | 阶乘寄存器：写入 n，设备后台算 n!，读回结果 |
| 0x20 | RW | status：bit0=正在计算（只读），bit7=算完发中断 |
| 0x24 | RO | 中断状态：哪些中断被触发 |
| 0x60 | WO | 触发中断 |
| 0x64 | WO | 应答中断（**不 ack 会中断风暴**） |
| 0x80 | RW | DMA 源地址（64 位） |
| 0x88 | RW | DMA 目标地址（64 位） |
| 0x90 | RW | DMA 传输长度 |
| 0x98 | RW | DMA 控制：0x1 启动 / 0x2 方向 / 0x4 完成发中断 |
| 0x40000 | — | 设备内部 4KB buffer 的"地址"，只给 DMA 用，CPU 不能直接读 |

**注意**：QEMU 的 `edu` 默认只支持 28 位 DMA 地址（会把高地址 clamp 掉），所以启动脚本用了
`-device edu,dma_mask=0xffffffff`，驱动里再声明 32 位。**这是理解"设备地址位宽"最好的例子**，面试常问。

---

## 4. 常用命令

```bash
# 虚拟机
bash scripts/start-vm.sh          # 起（幂等）
bash scripts/stop-vm.sh           # 关
tail -f ~/teco-vm/serial.log      # 串口日志（内核 panic 时看这里）

# 编译与部署（改完代码后跑这两条）
bash scripts/build.sh                        # 全部
bash scripts/build.sh week2-edu-probe        # 只编一个
bash scripts/deploy.sh

# guest 内（ssh 之后）
cd ~/driver-prep-mods
sudo dmesg -C                     # 清内核日志
sudo insmod edu_probe.ko
sudo dmesg | tail -10
sudo rmmod edu_probe
cat /proc/interrupts | grep edu
lspci -vv -s 00:04.0              # 看 edu 卡（编号以 lspci 为准）
ls /sys/kernel/debug/edu/         # week2~4 的 debugfs 接口
echo 10 | sudo tee /sys/kernel/debug/edu/factorial
```

---

## 5. 常见问题

| 现象 | 原因 / 解决 |
|---|---|
| `insmod: Invalid module format` | 内核版本变了：`bash scripts/build.sh` 会自动从 guest 读 `uname -r` 重新编译 |
| `build.sh` 报 `gcc-13: not found` | 脚本会自动 `sudo apt install gcc-13`；若失败手动装一次 |
| 首次 `build.sh` 下载头文件失败 | 网络问题，重跑即可；也可手动设 `KVER=6.8.0-147-generic` |
| `probe` 没被调用 | guest 里 `lspci -n` 应看到 `1234:11e8`；没有说明 QEMU 参数漏了 `-device edu` |
| 中断不触发（week3） | 忘了写 status 的 `0x80` 位（开 IRQFACT），或 handler 没 ack |
| DMA 超时（week4） | 忘了 `pci_set_master`，或 DMA 地址超过设备位宽（`dmesg` 有 clamp 警告） |
| 虚拟机启动很慢 | 本机无 KVM，TCG 模拟的正常现象（1~3 分钟） |
| 脚本报 `bad interpreter` | DOS 换行：`sed -i 's/\r$//' scripts/*.sh` |
| 想重来一遍 | `bash scripts/stop-vm.sh && rm -f ~/teco-vm/disk.qcow2 && bash scripts/start-vm.sh`（镜像与头文件不用重下） |

**可选：如果你想让 guest 自己也能编译**（需要 guest 能联网，慢）：
```bash
GUEST_BUILD=1 bash scripts/start-vm.sh
```
然后在 guest 内 `cd ~/driver-prep && make -C /lib/modules/$(uname -r)/build M=$PWD`。

---

## 6. 这套练习怎么映射到太初的 JD

| 太初 JD 原文 | 你这个项目里对应的事 |
|---|---|
| "负责自研 AI 芯片驱动组件的开发、维护与性能优化" | week2~4 就是一个最小加速卡驱动的开发全过程 |
| "参与芯片相关 IP 的微架构定义，协同完成设计与验证" | edu 卡定义了寄存器/BAR/DMA/中断的"契约"；你要理解驱动与硬件 IP 的接口设计 |
| "跟进 PCIe 技术、高速互联、设备虚拟化、内存管理" | week2 PCIe/BAR，week4 DMA/地址位宽；虚拟化（SR-IOV/VFIO）作为面试概念题 |
| "熟悉 Linux 设备驱动开发流程及内核机制" | 4 周全部围绕这一条 |
| 加分项：内核/PCIe 驱动/异构计算项目经历 | 简历："基于 QEMU edu 虚拟 PCIe 设备实现 Linux 加速卡驱动：MMIO 寄存器、MSI 中断、一致性 DMA 往返校验，debugfs 可触发" |

**面试时能讲的故事**（练完就能说）：
1. 一块 PCIe 设备从 `probe` 到 `remove`，驱动要管哪些资源、为什么顺序不能反；
2. MMIO 为什么必须用 `ioread32`、`__iomem` 是什么；
3. 中断上下文的限制、为什么要 ack、MSI 与 INTx 的区别；
4. DMA 地址为什么用 `dma_alloc_coherent` 返回的 handle、位宽不够时怎么办；
5. 多进程同时打开一张卡，驱动怎么管理（week5 扩展题，也是"内存/调度"岗日常）。

---

## 7. 下一阶段（4 周之后）

1. **week5：`ioctl` + `mmap` + 多进程** —— 自己设计 ioctl ABI，把设备 buffer `mmap` 给用户态，处理多进程 open/close 与引用计数。
2. **week6：性能分析** —— `perf`、`ftrace` 看中断延迟/上下文切换；理解工具链（TecoGDB/TSight）在做什么。
3. **week7：读真实代码** —— 内核 `drivers/net/ethernet/intel/e1000e/`（真实 PCIe 网卡驱动），对照 edu 驱动找差异。
4. **week8：简历与面试** —— 项目写成 STAR 结构，准备 5 个能讲 3 分钟的技术点。

---

## 7.5 PCIe 学习资料

- 仓库内资源索引：[docs/pcie-resources.md](docs/pcie-resources.md)（免费官方规范、Bootlin 培训、LDD3 章节、内核文档链接）
- 本地离线资料：`pcie-materials/`（含 Bootlin Linux PCI 驱动培训 PDF、LDD3 的 PCI/DMA 章节、内核文档快照、`drivers/pci/` 核心源码）
- 推荐顺序：第 1 周看体系结构（配置空间/BAR/枚举）→ 第 2 周中断/DMA/AER → 第 3 周 SR-IOV/P2P/真实驱动

---

## 8. 安全提示

- 所有实验都在**虚拟机**里做，出问题只影响 `~/teco-vm/disk.qcow2`，删了重来即可。
- 不要在 WSL 本体或 Windows 上执行 `insmod` 实验。
- 虚拟机使用用户态网络（NAT），只把 SSH 转发到本机 `127.0.0.1:2222`，不暴露到局域网。
