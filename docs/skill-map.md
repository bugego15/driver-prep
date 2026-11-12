# 个人技能树：AI 芯片 Host 侧驱动 / 内核方向

> **适用者**：rougu（嵌入式 · FPGA · SSD 端侧 → Host 侧 Linux 驱动转型）
> **目标岗位**：太初元碁（Tecorigin）
> - 主目标：**驱动研发工程师（内存/调度）**（基础软件类）
> - 备选：工具链研发工程师 / AI 软件架构（见 L6 分支）
> **核心目标岗位 JD 要点**（来源：太初 2027 届校招/社招公开信息）：
> - 负责自研 AI 芯片驱动组件的开发、维护与性能优化
> - 参与芯片相关 IP 的微架构定义，协同完成设计与验证
> - 跟进 PCIe、高速互联、设备虚拟化、内存管理及新特性
> - 要求：C/C++/Python/Shell、数据结构、计算机体系结构、**Linux 设备驱动与内核机制**
> - 加分：x86/ARM/RISC-V 汇编、内核/PCIe/异构计算项目、SR-IOV/VFIO/内存管理、操作系统/体系结构论文或竞赛
>
> **本文件用法**：这是"地图 + 进度表"，不是"通关清单"。每周自评一次，按 P0→P1→P2 顺序补。

---

## 0. 怎么用这份文档（先读这节）

### 0.1 四级熟练度（每项给自己打一个档）

| 档 | 名称 | 判断标准 |
|---|---|---|
| **A** | 知道 | 能说出"是什么、为什么"，面试被问不慌，但没动手 |
| **B** | 会用 | 能照着资料完成一次实践，遇到问题会查 |
| **C** | 独立做 | 能不看资料独立完成 + 能排错 + 有可展示的产出（代码/实验记录） |
| **D** | 能设计 | 能设计接口/方案、能讲清取舍、能带别人做、能回答"为什么这样设计" |

### 0.2 投简历的门槛（别等全学完）

- **P0 项**：必须到 **C**（有项目/实验证明），否则面试会被问穿；
- **P1 项**：到 **B**（能讨论设计，不要求做过）；
- **P2 项**：到 **A**（知道概念，能查资料）；
- **P3 项**：不要求，入职后学。

### 0.3 自评节奏

每周日：对着**第 13 节**的进度清单打勾 + 更新**第 10 节**的自评分。**只追 P0，别被 P2/P3 分散**。

### 0.4 完整性对照：原始技能树 → 本文档

本文档覆盖你贴出的 L0~L6 **全部条目**，共 **42 个可验收子项**，只是做了重新分组：

| 原始层 | 本文档章节 | 子项数 | 覆盖说明 |
|---|---|---|---|
| L0 语言与系统基础 | 第 2 节 | 4 | C / 构建调试 / Linux 用户态 API / Shell+Python |
| L1 Linux 内核 | 第 3 节 | 7 | 模块 / 字符设备与 UAPI / 内核内存 / 并发 / 中断 / 调试设施 / 其他 |
| L2 硬件与总线 | 第 4 节 | 9 | PCIe / 资源 / 中断 / DMA / IOMMU / RAS / P2P / CPU 与内存序 / 链路系统 |
| L3 加速器专属 | 第 5 节 | 8 | 驱动模型 / 设备内存 / 调度 / 固件 / 多进程 / 性能 / 多卡通信 / 负载 |
| L4 虚拟化 | 第 6 节 | 5 | SR-IOV / VFIO / mdev / KVM / 算力切分 |
| L5 调试验证 | 第 7 节 | 4 | 内核调试 / GDB 内部 / 硬件调试 / 测试验证 |
| L6 工具链研发 | 第 8 节 | 5 | TecoGDB / TSight / SMI / TCVS / 编译器与构建 |

> 说明：原始 L4 的"SR-IOV、VFIO、mdev、KVM、IOMMU group、直通"六点，合并为 5 个子项（IOMMU group 并入 VFIO 与隔离部分）；原始 L1 的"misc 设备"并入 L1.1。

---

## 1. 你的现状盘点（2026-11 基线）

### 1.1 已有资产（这些不用重学，要写进简历）

| 资产 | 来源 | 对应技能层 |
|---|---|---|
| 嵌入式 C 编程（指针/内存/位操作/寄存器） | 多年 SSD 固件/端侧开发 | L0.1 |
| Linux 用户态开发、Shell、Git | 日常工作 | L0.3 / L0.4 |
| FPGA 开发（Vivado、时序、AXI、JTAG 调试、串口） | 采集板项目 | L2 硬件直觉、L5.3 |
| PCIe 采集系统 host 侧数据通路（XDMA、DMA 压测、多卡并发、fio、测试文档） | 6dongcai 项目 | L2.1~L2.4 的工程经验 |
| 设备侧视角（固件、寄存器、硬件手册） | SSD 端侧 | L2/L3 的差异化优势 |
| QEMU edu 虚拟卡驱动实践（PCI/BAR/MMIO/MSI/DMA） | driver-prep week1~4 | L1/L2 入门 |
| 实机 PCIe 观察（lspci -vv、/proc/iomem、/sys resource、链路速率） | 工控机 192.168.120.49 | L2.1/L2.2/L2.9 |

### 1.2 当前自评（基线）

| 层 | 当前档位 | 说明 |
|---|---|---|
| L0 语言与系统 | **C** | C 熟练；GDB 基础；Shell/Python 够用 |
| L1 Linux 内核 | **B-** | 能写模块（week1~4），并发/内存管理/子系统经验不足 |
| L2 硬件与总线 | **B-** | PCIe 概念 + 一次真实观察 + DMA 实践；AER/IOMMU/P2P 未碰 |
| L3 加速器专属 | **A-** | 知道 context/调度/统一内存概念，没写过 |
| L4 虚拟化 | **A** | 知道 SR-IOV/VFIO 是什么 |
| L5 调试验证 | **B-** | JTAG/GDB 用过，ftrace/perf/eBPF 没系统练 |
| L6 工具链 | **A-** | 了解 TecoGDB/TSight/SMI/TCVS 定位 |

### 1.3 明确不做什么（防止焦虑）

- ❌ 不要通读 PCIe 5.0/6.0 规范（用到再查）；
- ❌ 不要一开始啃 amdgpu/i915 这种百万行驱动；
- ❌ 不要为了 L6 工具链分心（除非确定转工具岗）；
- ❌ 不要追求"六层全 C"，P0 三个 C 就够投。

---

## 2. L0 语言与系统基础（优先级 **P0**）

### L0.1 C（内核风格 C）— 目标 C 档（已有基础，查漏）

**内容清单**
- 指针与数组退化、函数指针、回调、`container_of` 思想
- 内存布局：栈/堆/静态区、结构体对齐与 padding、位域
- 位运算、字节序、类型宽度（`u8/u16/u32/u64`）
- 链表（内核 `list_head`）、哈希表（`hlist`）、环形缓冲（ring buffer）
- 宏与 `__attribute__`（`__packed`、`aligned`、`section`）
- 编译器视角：`volatile`、`barrier()`、`READ_ONCE/WRITE_ONCE`

**达标自测（C）**
1. 不用库，手写一个无锁/单生产者单消费者环形缓冲；
2. 解释 `container_of(ptr, type, member)` 的原理并手写一遍；
3. 解释结构体对齐规则，给出 `sizeof` 计算结果；
4. 解释 `volatile` 在驱动里为什么不够、`READ_ONCE` 解决什么。

**资源**：LDD3 第 9/11 章、内核 `include/linux/list.h`、`include/linux/compiler.h`

**练习**：把 edu 驱动里的 debugfs 数值解析改成更健壮的 `kstrtou32_from_user` 风格。

---

### L0.2 构建与调试工具 — 目标 C 档

**内容清单**
- `make` + Kbuild（`obj-m`、`M=`、`EXTRA_CFLAGS`）
- CMake（用户态工具/测试）
- `git`（分支、rebase、log/blame、bisect）
- 用户态 GDB（断点、watch、backtrace、core、`info registers`）
- `objdump`、`readelf`、`nm`、`strace`、`ltrace`

**达标自测**
1. 能解释 `.ko` 的 ELF 结构（`.text`/`.modinfo`/`.gnu.linkonce.this_module`）；
2. 能用 `strace` 定位用户态程序卡在哪个 syscall；
3. 能用 `git bisect` 找到引入 bug 的提交。

**练习**：给 driver-prep 的 Makefile 加 `ccflags-y`、`V=1` 调试。

---

### L0.3 Linux 用户态 API — 目标 C 档

**内容清单**
- `open/close/read/write/ioctl/mmap/poll/epoll`
- `fork/exec/wait`、信号、进程间通信（管道/共享内存/socket）
- `/proc`、`/sys` 文件读取
- 内存映射与 `mmap` 语义（`MAP_SHARED`/`MAP_PRIVATE`、page fault）
- 权限：`CAP_SYS_ADMIN`、`udev`、`/dev` 权限

**达标自测**
1. 解释 `mmap` 后第一次写为什么可能触发缺页；
2. 解释 `ioctl` 的 `_IOW/_IOR/_IOWR` 编码规则；
3. 能写一个 `epoll` 服务端处理多设备事件。

**练习**：给 edu 驱动写一个用户态测试程序（`open/ioctl/mmap`），替代 shell debugfs 操作。

---

### L0.4 Shell / Python — 目标 B 档

**内容清单**
- Shell：管道、重定向、`set -euo pipefail`、`awk/sed/grep`
- Python：`argparse`、`subprocess`、`json`、`re`、`requests/paramiko`
- 测试脚本：自动化跑用例、收集日志、生成报告

**达标自测**：能写一个自动"加载模块→跑测试→抓 dmesg→卸载→出报告"的脚本。

**练习**：把 driver-prep 的 smoke-test 改造成 Python 报告（JSON + 失败项高亮）。

---

## 3. L1 Linux 内核（优先级 **P0**，面试核心）

### L1.1 模块与驱动模型 — 目标 C 档

**内容清单**
- 模块生命周期：`module_init/exit`、`__init/__exit`、引用计数、`rmmod` 失败原因
- 总线/设备/驱动模型：`bus_type`、`device`、`driver`、`probe/remove`
- 匹配机制：`id_table`、`modalias`、`uevent`
- 资源管理：`devm_*`、`class_create/device_create`
- **misc 设备**（`misc_register`）——功能单一的设备首选，比完整 cdev 省事
- 模块参数：`module_param`、`MODULE_PARM_DESC`

**达标自测**
1. `insmod` 到 `probe` 的完整调用链（说出 5 个函数）；
2. `devm_kzalloc` 为什么能自动释放（`devres` 机制）；
3. 设备节点 `/dev/xxx` 是怎么创建的（`cdev_add` + `device_create`）。

**资源**：LDD3 第 2/14 章；内核 `drivers/base/`、`fs/char_dev.c`

**练习**：edu 驱动从 `debugfs` 升级为**真正的字符设备**（`cdev` + `class_create`）。

---

### L1.2 字符设备与 UAPI 设计 — 目标 C 档（week5 核心）

**内容清单**
- `file_operations`：`open/release/read/write/unlocked_ioctl/mmap/poll`
- `ioctl` 设计：号段分配、参数拷贝（`copy_from_user`）、版本兼容、保留字段
- `mmap`：`remap_pfn_range`、`vm_insert_page`、`VM_IO/VM_DONTEXPAND`
- 多进程：`open` 引用计数、文件私有数据、进程退出清理
- UAPI 头文件：`include/uapi/linux/xxx.h`、`#ifdef __KERNEL__`

**达标自测**
1. 设计一个"提交任务 + 等待完成"的 ioctl 接口（含结构体/错误码）；
2. 解释 `mmap` 设备内存的两种实现及安全性差异；
3. 两个进程同时打开设备，如何保证资源独立/共享正确；
4. `ioctl` 里为什么不能直接解引用用户指针。

**资源**：LDD3 第 6 章（ioctl）、第 15 章（mmap）、内核 `Documentation/userspace-api/`

**练习**：**week5** — 给 edu 驱动加 `ioctl`（提交阶乘/DMA）+ `mmap` 设备 buffer + 多进程文件私有数据。

---

### L1.3 内核内存管理 — 目标 C 档

**内容清单**
- 分配器：`kmalloc/kzalloc`、`vmalloc`、`alloc_pages/get_free_pages`、`kcalloc`
- slab/SLUB 概念、内存池（`mempool`）、DMA 内存（见 L2.4）
- 用户页 pin：`pin_user_pages`、`get_user_pages`
- 页表/虚拟地址：`virt_to_phys`、`page_address`、`kmap`
- 内存泄漏/越界排查：`KASAN`、`slub_debug`

**达标自测**
1. `kmalloc` vs `vmalloc` 的区别（物理连续？可睡眠？大小限制？）；
2. 为什么 DMA 不能用 `virt_to_phys`（引出 IOMMU/一致性）；
3. 内核里内存耗尽会怎样（OOM killer 管不管内核分配？）。

**资源**：LDD3 第 8/15 章、`Documentation/core-api/memory-allocation.rst`、`mm/slab.c`

**练习**：给 edu 驱动加一个按页分配的 buffer，用 `vmalloc` 做大块实验并对比。

---

### L1.4 并发与同步 — 目标 C 档（面试重灾区）

**内容清单**
- 锁：`spinlock_t`、`mutex`、`rwlock`、`seqlock`、`rcu_read_lock`
- 原子操作：`atomic_t`、`refcount_t`、`cmpxchg`
- 等待队列：`wait_event_*`、`complete`
- 工作队列：`workqueue`、`tasklet`、`softirq`、`threaded IRQ`
- 上下文禁忌：中断上下文不能睡眠；原子上下文不能拿 mutex
- 死锁：ABBA、锁顺序、`lockdep`

**达标自测**
1. 自旋锁 vs mutex 的选择标准（能否睡眠、临界区长度）；
2. RCU 适合什么场景？读写者各怎么做；
3. 中断 handler 里要"做耗时操作"怎么办（threaded IRQ / workqueue / tasklet 区别）；
4. 一个真实驱动里会用到哪几种同步原语。

**资源**：LDD3 第 5 章、`Documentation/locking/`、`include/linux/spinlock.h`

**练习**：给 edu 驱动加一个多用户并发测试，用 `lockdep` 检查锁顺序。

---

### L1.5 中断处理 — 目标 C 档（week3 已覆盖基础）

**内容清单**
- 中断注册：`request_irq/request_threaded_irq`、`IRQF_SHARED/IRQF_ONESHOT`
- 上半部/下半部、threaded handler
- 中断亲和性、`/proc/interrupts`、`IRQ_NONE` 语义
- 中断风暴、伪中断、共享中断判断

**达标自测**
1. 写出一次完整中断处理流程（硬件→CPU→handler→下半部）；
2. `IRQ_NONE` 什么时候返回？返回错会怎样；
3. 共享中断里如何判断"是不是我的中断"。

**资源**：LDD3 第 10 章、`Documentation/core-api/genericirq.rst`、`kernel/irq/`

**练习**：week3 已做；再补一个 **threaded IRQ** 版本对比吞吐。

---

### L1.6 内核调试基础设施 — 目标 B 档

**内容清单**
- `printk` 等级、`dev_*` 宏、动态调试（`dynamic_debug`）
- `debugfs`/`sysfs`/`procfs` 分工与安全
- `ftrace`（function_graph、事件、trace_pipe）
- `perf`（内核视角）、`tracepoint`、`kprobe`
- `dmesg` 解读、`panic/oops` 信息结构

**达标自测**
1. 给驱动加 tracepoint 并在 `trace_pipe` 看到；
2. 用 `ftrace function_graph` 测量一个 ioctl 的耗时；
3. oops 日志里如何找"出错的函数和地址"（符号化）。

**练习**：week6 的迷你 profiler 项目（见 L5/L6）。

---

### L1.7 其他内核设施 — 目标 A/B 档

- `kthread`（内核线程）、`completion`、定时器/`hrtimer`
- `cgroup`/`namespace`（多租户隔离基础，见 L4）
- 电源管理：`dev_pm_ops`、`runtime PM`
- 模块签名/secure boot（了解）

**达标自测**：能解释 `cgroup` 如何限制一个进程组的 GPU/内存资源（为算力配额做铺垫）。

---

## 4. L2 硬件与总线（优先级 **P0**，你的差异化优势）

### L2.1 PCIe 协议与配置空间 — 目标 C 档

**内容清单**
- 拓扑：domain/bus/device/function、桥、交换芯片、Root Complex
- 配置空间：256B 标准头、4KB 扩展、Capability 链、ECAM vs CF8/CFC
- 关键寄存器：BAR0~5、Command/Status、Class、Subsystem、PCIe Cap
- BAR 探测（写全 1 读回）、资源分配、桥窗口包含关系
- 链路：LnkCap/LnkSta、速率 GT/s、宽度、训练、retimer

**达标自测**
1. `lspci -vv` 输出逐字段解释（BAR/LnkCap/LnkSta/Capabilities）；
2. 解释 `/proc/iomem` 中 `PCI Bus` 窗口与 BAR 的包含关系（已在工控机实操）；
3. BAR 大小如何计算？64 位 BAR 用几个寄存器？
4. ECAM 窗口大小怎么算（256 总线 × 1MB）？

**资源**：本仓库 `pcie-materials/`（Bootlin 讲义、LDD3 ch12、内核 docs、工控机观察记录）
**练习**：已完成（lspci -x、/sys resource、iomem 解析）→ 下一步读 `drivers/pci/probe.c`、`pci-driver.c`。

---

### L2.2 BAR 与资源管理 — 目标 C 档

**内容清单**
- `pci_resource_start/end/flags`、`IORESOURCE_*` 位
- `pci_request_regions`/`pci_iomap`/`pci_iounmap`
- 资源冲突 `-EBUSY`、`/proc/iomem` 定位
- prefetchable 位、64 位 BAR、大 BAR（AI 卡显存窗口）

**达标自测**
1. 写一段代码：遍历设备 BAR、打印地址/大小/flags；
2. 解释为什么 GPU 的 BAR1 通常是 GB 级 64 位 prefetchable；
3. 资源冲突时驱动应该怎么处理。

**练习**：给 edu 驱动加"资源信息"debugfs 节点（模仿 `/sys resource`）。

---

### L2.3 中断（MSI/MSI-X）— 目标 C 档

**内容清单**
- INTx vs MSI vs MSI-X 原理与配置
- `pci_alloc_irq_vectors`、`pci_irq_vector`、`pci_free_irq_vectors`
- MSI-X 表（BAR 内）、多向量分配、亲和性
- 中断聚合/poll 模式（高吞吐场景）

**达标自测**
1. 解释 MSI-X 表和 PBA 在哪里、怎么配置；
2. 多向量中断如何映射到不同处理函数（`request_irq` 多个）；
3. 为什么 AI 卡流行"中断 + 轮询"混合模式。

**资源**：`Documentation/PCI/msi-howto.rst`、`drivers/pci/msi/`
**练习**：week3 已做 → 扩展为多向量（4 个 IRQ）实验。

---

### L2.4 DMA — 目标 C 档（分水岭）

**内容清单**
- 一致性 vs 流式 DMA、`dma_alloc_coherent`、`dma_map_single/sg`、`dma_sync_*`
- DMA mask/位宽（`dma_set_mask_and_coherent`）、IOMMU 交互、bounce buffer
- 描述符环、scatter-gather、doorbell、完成轮询/中断
- 设备内存 vs 主机内存、P2P DMA（见 L2.7）

**达标自测**
1. 为什么 DMA 地址要用 `dma_alloc_*` 返回的 handle，而不是 `virt_to_phys`；
2. 一致性 DMA 和流式 DMA 的 cache 处理差异；
3. 写出一个"提交 DMA → 等完成 → 校验"的流程（含错误路径）。

**资源**：LDD3 第 15 章（待精读）、`Documentation/core-api/dma-api*`
**练习**：week4 已做 → 扩展 SG 列表 + 多次传输 + 性能测量。

---

### L2.5 IOMMU — 目标 A/B 档

**内容清单**
- 地址翻译：设备虚拟地址→物理地址；x86 VT-d / ARM SMMU / RISC-V IOMMU
- IOMMU group、直通、`iommu=pt`、DMAR ACPI 表
- 与 DMA API/`dma_map_*` 的关系、SWIOTLB
- 安全意义：多租户隔离（一个设备不能访问别人内存）

**达标自测**
1. 解释 `/proc/iomem` 里 `dmar0/dmar1` 是什么（已实测）；
2. 没有 IOMMU 时 DMA 有什么风险；
3. 为什么 VFIO 依赖 IOMMU group。

**练习**：在 guest 里查看 IOMMU group（`ls /sys/kernel/iommu_groups/`），讲解分组规则。

---

### L2.6 RAS / AER / 复位 — 目标 B 档（JD 相关）

**内容清单**
- AER：Correctable/Non-Fatal/Fatal、错误上报、DPC
- 掉卡场景：AER→驱动感知→恢复流程（FLR/热复位/重启）
- ECC、故障隔离、日志
- 热插拔（`pciehp`）

**达标自测**
1. 一块卡 AER 报错，驱动的恢复顺序是什么；
2. 为什么要"先停 DMA 再复位"；
3. `/sys/bus/pci/devices/*/aer_dev_correctable` 怎么读。

**练习**：给 edu 驱动加"错误注入 + 恢复"模拟（用软件标志模拟，不动硬件）。

---

### L2.7 P2P / ATS / PRI / ACS — 目标 A 档（加分）

**内容清单**
- P2P DMA（卡↔卡）、`pci_p2pdma`、BAR1 aperture
- ACS（访问控制）与 P2P 的关系
- ATS/PRI/PASID（共享虚拟内存基础）
- 拓扑感知（NUMA、PCIe 树距离）

**达标自测**
1. 解释"卡间通信为什么不走主机内存"；
2. ACS 关闭为什么能让 P2P 通；
3. ATS 解决了什么问题（地址翻译缓存）。

**资源**：`Documentation/driver-api/pci/p2pdma.rst`、`drivers/pci/p2pdma.c`、`drivers/pci/iov.c`

---

### L2.8 CPU 架构与内存序 — 目标 B 档

**内容清单**
- x86/ARM/RISC-V 差异（弱内存序、cache 一致性）
- 内存屏障：`mb/rmb/wmb`、`smp_mb`、acquire/release
- 汇编阅读（至少能看懂函数序言、寄存器传参、`cpuid` 类指令）
- RISC-V：太初芯片内有 RISC-V 核（了解基本 ISA）

**达标自测**
1. 解释 x86 TSO vs ARM 弱序对驱动的影响；
2. 设备写寄存器后为什么可能需要 `mmiowb`/屏障；
3. 能看懂 objdump 出的简单函数。

**练习**：读 `Documentation/memory-barriers.txt` 前 1/3，做笔记。

---

### L2.9 链路与系统集成 — 目标 B 档

- 链路速率/宽度/降速、retimer、信号完整性（了解）
- PCIe 枚举顺序（内核 log 解读）
- 多卡系统的拓扑设计（switch、NUMA、带宽瓶颈）
- 电源（ASPM）、热设计

**达标自测**：拿到 `lspci -vv` + `/proc/iomem` + `dmesg`，能画出整机 PCIe 拓扑图并指出瓶颈。

**练习**：用已有工控机观察记录画一张拓扑图（已部分完成）。

---

## 5. L3 加速器专属（优先级 **P1**，最稀缺）

### L3.1 GPU/加速器驱动模型 — 目标 B 档

**内容清单**
- context、queue（命令队列）、command buffer、doorbell
- 设备内存对象、handle/VM 地址空间
- 固件加载与启动状态机、device reset、FLR
- 多进程共享一张卡（fd/context/资源生命周期）
- 主流实现：`drivers/accel/qaic`（小）、`drivers/accel/habanalabs`（大）

**达标自测**
1. 画出"用户程序→runtime→驱动→设备"的命令提交时序；
2. 解释 context 切换时驱动要做什么；
3. 进程异常退出（kill -9）时驱动怎么清理设备资源。

**资源**：本地 `kernel-src/`、`drivers/accel/`（需下载）、`pcie-materials/html/`
**练习**：读 `drivers/accel/qaic` 的 UAPI 与主流程，写阅读笔记。

---

### L3.2 设备内存管理 — 目标 B 档（岗位核心）

**内容清单**
- 显存分配器（buddy/slab/池化、碎片、OOM）
- 设备页表/MMU、地址空间隔离、缺页处理
- 统一内存：UVA、HMM、按需迁移、`migrate_vma`
- 大页、异构内存（HBM + 主机内存）、冷热分级
- 跨进程共享（dma-buf、IPC）

**达标自测**
1. 设计一个"显存分配器"接口（分配/释放/查询/碎片统计）；
2. 解释 page fault 驱动迁移的流程（HMM 大致步骤）；
3. 多租户下如何限制一个进程的显存。

**练习**：给 edu 驱动加"设备内存池"（模拟显存），实现 alloc/free + 统计。

---

### L3.3 调度器设计 — 目标 B 档（岗位核心）

**内容清单**
- 多队列调度、优先级、时间片、抢占
- QoS：延迟敏感 vs 吞吐
- 算力配额/切分（SR-IOV/MIG 思想）
- 多进程公平性、饥饿、优先级反转

**达标自测**
1. 设计一个"8 个 context 共享 4 个队列"的调度方案；
2. 时间片轮转 vs 优先级抢占的取舍；
3. 如何统计每个进程的算力占用（为计费/配额）。

**练习**：在 edu 驱动里模拟一个多队列调度器（软件队列 + 定时器）。

---

### L3.4 固件与启动 — 目标 B 档

- FW 加载（`request_firmware`）、校验、版本
- 设备启动状态机、初始化超时、失败恢复
- FW update 流程（安全、回滚）

**达标自测**：描述"驱动加载→固件下载→设备就绪"的完整流程及失败分支。

---

### L3.5 多进程/多租户 — 目标 B 档

- 引用计数、fd 生命周期、进程退出清理
- 全局 vs 私有资源（共享 VM、独占队列）
- OOM/配额、安全隔离
- 与 cgroup 的结合

**练习**：week5 的多进程部分 + 一个"配额"扩展。

---

### L3.6 性能分析（AI 加速器） — 目标 B 档

- Roofline、带宽 vs 计算、占用率
- PCIe 带宽瓶颈、DMA 效率、中断开销
- 工具：厂商 profiler（TSight 类）、`perf`、自研采样器
- 训练/推理负载特征（batch、序列长度、并行策略）

**达标自测**：拿到一个"卡很慢"的场景，能列出排查顺序（先看链路/中断/内存/调度哪个环节）。

---

### L3.7 多卡通信与 P2P — 目标 A/B 档

- 集合通信（all-reduce/all-gather）与并行策略（DP/TP/PP）
- 卡间链路（NVLink/统一 LINK/光互联）、PCIe P2P 兜底
- 通信库接口（NCCL 类）与驱动的关系
- 拓扑感知调度

**达标自测**：解释"张量并行为什么每 token 都要通信"。

---

### L3.8 AI 工作负载基础 — 目标 A 档

- 训练 vs 推理差异；KV cache；MoE；量化
- 为什么推理需要低延迟、训练需要高吞吐
- 常见框架：PyTorch、vLLM、SGLang、Megatron
- 目的：**理解你的驱动在支撑什么**，面试时能聊业务

---

## 6. L4 虚拟化（优先级 **P2**，JD 加分）

### L4.1 SR-IOV — 目标 B 档

**内容清单**
- PF/VF 概念、VF 使能流程（`sriov_numvfs`）
- VF 的 BAR/配置空间/资源分配
- VF 与 PF 的关系、FLR 对 VF 的影响

**达标自测**
1. 一张物理卡怎么虚拟出多个 VF？VF 对主机看起来像什么？
2. VF 的 BAR 和 PF 是独立还是共享？
3. 写 `sriov_numvfs` 时内核做了什么？

**资源**：`Documentation/PCI/pci-iov-howto.rst`、`drivers/pci/iov.c`

### L4.2 VFIO 与设备直通 — 目标 B 档

- VFIO 框架：`vfio-pci`、`vfio-group`、container
- **IOMMU group** 的概念与分组规则
- 直通设备给虚拟机（QEMU `-device vfio-pci`）

**达标自测**：解释为什么 VFIO 依赖 IOMMU group；把一个 PCI 设备直通给 VM 的步骤。

**资源**：`drivers/vfio/`、`Documentation/driver-api/vfio.rst`

### L4.3 mdev（软件切分）— 目标 A 档

- mediated device 框架、厂商实现（如 vGPU）
- 与 SR-IOV 的取舍

**达标自测**：什么场景用 mdev 而不是 SR-IOV？

### L4.4 KVM 基本流程 — 目标 A 档

- KVM 的 VM/vCPU 概念、`/dev/kvm` ioctl
- 与设备直通/virtio 的关系

**达标自测**：一个 VM 启动时，设备和内存是怎么被 KVM 管理的？

### L4.5 算力切分与多租户隔离 — 目标 B 档

- 切分维度：内存、算力、带宽、中断
- 配额/计量、隔离边界

**达标自测**：给一张 AI 卡设计"切成 4 份"的方案，列出要实现哪些隔离。

**资源汇总**：`drivers/pci/iov.c`、`drivers/vfio/`、`Documentation/PCI/pci-iov-howto.rst`
**练习**：读 iov.c + 用 `lspci` 观察 SR-IOV 网卡（工控机 I210 支持）。

---

## 7. L5 调试与验证（优先级 **P2/P3**，工具岗才 P1）

### L5.1 内核调试 — 目标 B 档

- `kdump`/`crash`、oops 分析、`addr2line`
- `ftrace`/`perf`/`eBPF`（bpftrace）
- KASAN/KCSAN/KMSAN、`slub_debug`、lockdep
- `syzkaller` 模糊测试（了解）

**达标自测**：能独立分析一次 oops（找到函数、行号、调用栈）。

---

### L5.2 GDB 内部与调试器原理 — 目标 A/B 档

- ptrace、断点（INT3）、单步
- DWARF 调试信息结构
- RSP 协议（GDB remote serial protocol）
- core dump 生成/分析

**练习**：写一个最简单的 GDB remote stub（见 L6）。

---

### L5.3 硬件调试 — 目标 B 档（你的优势）

- JTAG/OpenOCD、边界扫描
- 示波器/逻辑分析仪读 PCIe 信号（了解即可）
- 板级问题定位（电源、时钟、复位、链路）

**优势**：你已有 FPGA/JTAG 经验 → 这块比纯软件候选人强，面试可讲。

---

### L5.4 测试与验证 — 目标 B 档

- 单元测试（KUnit）、回归、压力测试
- 故障注入（RAS）、长稳测试
- CI（GitHub Actions 编译、QEMU 测试）
- 覆盖率、静态分析（smatch/sparse）

**练习**：给 driver-prep 加 GitHub Actions（编译 + smoke test）。

---

## 8. L6 工具链研发（优先级 **P3**；投工具岗则 P1）

> 只有确定投"工具链研发工程师"时才深挖；否则做到 A 档（知道每个工具做什么）即可。

### L6.1 调试器工具（TecoGDB 类）
- GDB 二次开发、Python 扩展、远程 stub、DWARF
- 已有练习：L5.2 的迷你 stub

### L6.2 性能工具（TSight 类）
- `perf_event_open`、PMU、tracepoint、采样缓冲
- 时间线数据格式、可视化（Perfetto/Qt/TUI）
- 对标 nsys/ncu 的功能拆解

### L6.3 系统管理工具（TECO-SMI 类）
- 读 sysfs/debugfs、CLI/JSON 输出、Python 绑定
- 多卡/多进程、权限、复位/ECC 操作
- 练习：**edu-smi 小项目**（见第 9 节）

### L6.4 验证工具（TCVS 类）
- 寄存器 dump、DMA/链路压力、RAS 注入、产线自检

### L6.5 编译器/构建工具链
- LLVM/Clang 后端概念（了解）、汇编/反汇编、SDK 打包、CI

---

## 9. 优先级总表 & 12 周路线

### 9.1 优先级总表

| 优先级 | 技能项 | 目标档 |
|---|---|---|
| **P0** | L0.1~L0.4、L1.1~L1.5、L2.1~L2.4 | C |
| **P1** | L1.6、L2.5、L2.6、L2.8、L3.1~L3.6 | B |
| **P2** | L2.7、L2.9、L4、L5.1、L5.4 | A~B |
| **P3** | L5.2、L5.3、L6.* | A（工具岗另算） |

### 9.2 12 周路线（与 driver-prep 对齐）

| 周 | 主题 | 产出 | 覆盖技能 |
|---|---|---|---|
| 1~2（已完成） | QEMU edu 驱动：PCI/BAR/MMIO | week1~2 代码 | L0、L1.1、L2.1、L2.2 |
| 3~4（已完成） | 中断 + DMA | week3~4 代码 | L1.5、L2.3、L2.4 |
| 5 | 字符设备 + ioctl + mmap + 多进程 | **week5** | L1.2、L1.3、L1.4、L3.5 |
| 6 | 资源/统计/多队列调度器 | week6 | L3.2、L3.3 |
| 7 | 读 `nvme/host/pci.c` + `accel/qaic` | 阅读笔记 | L2.5、L3.1 |
| 8 | AER/FLR/错误恢复实验 | week7 | L2.6 |
| 9 | 迷你 profiler（perf_event_open） | 工具项目 A | L5.1、L6.2 |
| 10 | `edu-smi`（sysfs CLI/JSON） | 工具项目 B | L6.3 |
| 11 | SR-IOV/IOMMU 概念专题 + 文档 | 专题笔记 | L4、L2.5、L2.7 |
| 12 | 简历/面试模拟/项目复盘 | 简历 + 项目文档 | 全部 |

---

## 10. 面试自评表（每月更新）

| 层 | 目标 | 当前 | 差距 | 下一步动作 |
|---|---|---|---|---|
| L0 | C | C | 无 | 保持 |
| L1 | C | B- | 并发/内存/UAPI 实战 | week5 |
| L2 | C | B- | AER/IOMMU/P2P | 第 7~8 周 |
| L3 | B | A- | 全部 | week5~6 + 读 qaic |
| L4 | A | A | — | 第 11 周专题 |
| L5 | B | B- | ftrace/perf | 第 9 周 |
| L6 | A（工具岗 B） | A- | — | 第 10 周 |

**换算规则**：P0 项全 C + P1 项全 B → 可以投简历；否则先补最弱的一个 P0。

---

## 11. 简历项目映射（把技能变成一句话）

| 项目 | 简历写法 | 命中技能 |
|---|---|---|
| edu 驱动 week1~4 | "基于 QEMU edu 虚拟 PCIe 设备实现 Linux 加速卡驱动：BAR/MMIO 寄存器、MSI 中断、一致性 DMA 往返校验，debugfs 触发" | L1、L2.1~L2.4 |
| edu 驱动 week5 | "设计字符设备 UAPI（ioctl/mmap），支持设备内存映射与多进程共享，处理 fd 引用计数与退出清理" | L1.2、L3.5 |
| 工控机观察 | "分析真实 PCIe 拓扑：PEX8749 交换芯片、8 下游端口、BAR 资源树、/proc/iomem 窗口包含关系" | L2.1、L2.9 |
| 6dongcai | "8 卡 FPGA 采集系统 host 侧：XDMA 数据通路、DMA 压测、多卡并发、测试交付" | L2.4、L0 |
| 工具项目 | "实现 edu-smi 管理工具 / 迷你 profiler（perf_event_open 采样），支持 JSON 输出与失败诊断" | L5、L6 |

---

## 12. 资源索引

### 本地（本仓库）
- `pcie-materials/pdf/`：Bootlin PCI 驱动培训、LDD3 ch12/ch15
- `pcie-materials/html/`：内核文档快照（PCI/MSI/AER/SR-IOV/P2P/edu）
- `pcie-materials/kernel-src/`：`drivers/pci/` 核心源码 + nvme host
- `pcie-materials/中文讲义/`：Bootlin 精讲、LDD3 ch12 精读
- `pcie-materials/observations/`：工控机实机观察记录
- `docs/concepts-checklist.md`：每周自测题
- `docs/pcie-resources.md`：在线资源链接

### 在线
- 内核文档：https://docs.kernel.org/
- 内核源码交叉引用：https://elixir.bootlin.com/linux/latest/source
- PCI-SIG 规范：https://pcisig.com/specifications
- LDD3 免费：https://lwn.net/Kernel/LDD3/

---

## 13. 进度追踪（每周日勾选）

### 第 1~4 周（已完成）

- [x] QEMU + VM 环境搭建（WSL 编译 + guest 加载）
- [x] week1 最小模块
- [x] week2 PCI probe / BAR / MMIO / debugfs
- [x] week3 MSI 中断
- [x] week4 一致性 DMA
- [x] 实机 PCIe 观察（lspci/iomem/sys resource）

### 第 5 周：字符设备 + UAPI

- [ ] 字符设备（cdev + class_create + /dev 节点）
- [ ] ioctl 设计（提交任务/查询状态/错误码）
- [ ] mmap 设备内存（remap_pfn_range 或 vm_insert_page）
- [ ] 多进程 open/close 引用计数与退出清理
- [ ] 用户态测试程序（open/ioctl/mmap）
- [ ] 自测：设计题第 1~4 题（见 0.1 节 C 档标准）

### 第 6 周：内存与调度

- [ ] 设备内存分配器（池化 + 统计）
- [ ] 多队列调度器（优先级 + 时间片模拟）
- [ ] 配额统计
- [ ] 自测：分配器/调度设计题

### 第 7 周：读真实驱动

- [ ] 通读 `drivers/nvme/host/pci.c`（队列/MSI-X/DMA/复位）
- [ ] 读 `drivers/accel/qaic`（UAPI/context/内存）
- [ ] 写对照笔记（edu vs nvme vs qaic）

### 第 8 周：RAS

- [ ] AER/恢复流程实验（软件模拟）
- [ ] 故障注入测试
- [ ] 自测：掉卡恢复设计题

### 第 9~10 周：工具

- [ ] 迷你 profiler（perf_event_open）
- [ ] edu-smi（sysfs + JSON）
- [ ] 工具 README/截图

### 第 11 周：虚拟化/IOMMU 专题

- [ ] SR-IOV 概念 + iov.c 阅读
- [ ] IOMMU group 实验
- [ ] 专题笔记

### 第 12 周：面试冲刺

- [ ] 简历（项目 STAR）
- [ ] 8 个设计题能讲 3 分钟
- [ ] 模拟面试（我出题）
