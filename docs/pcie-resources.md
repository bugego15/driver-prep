# PCIe 学习资源索引（仓库版）

> 完整的离线资料在本地目录 `pcie-materials/`（不进 Git，避免仓库变大）。
> 这里只保留**可公开访问的链接**，换台机器也能照着找回来。

## 免费且合法（推荐）

| 资源 | 链接 | 用途 |
|---|---|---|
| PCI-SIG 官方规范 | https://pcisig.com/specifications | 最权威教材，注册后免费下载（Base Spec / MSI-X / AER / SR-IOV） |
| Bootlin Linux PCI 驱动培训 PDF | https://bootlin.com/doc/legacy/pci-drivers/pci-drivers.pdf | 驱动开发全流程，配内核代码 |
| LDD3 第 12 章（PCI 驱动） | https://lwn.net/images/pdf/LDD3/ch12.pdf | 驱动骨架、BAR、资源 |
| LDD3 第 15 章（内存映射与 DMA） | https://lwn.net/images/pdf/LDD3/ch15.pdf | DMA 概念原始出处 |
| Linux 内核 PCI 文档 | https://docs.kernel.org/PCI/index.html | 驱动指南 / MSI / AER / SR-IOV |
| Linux DMA-API 指南 | https://docs.kernel.org/core-api/dma-api.html | DMA 必读 |
| P2P DMA 文档 | https://docs.kernel.org/driver-api/pci/p2pdma.html | 卡间直连 |
| TI Precision Labs PCIe 视频 | https://www.ti.com/video/series/precision-labs/ti-precision-labs-pcie.html | 免费入门课 |
| Bootlin 培训材料总库 | https://bootlin.com/docs/ | 内核/驱动/实时 |
| QEMU edu 设备规范 | https://www.qemu.org/docs/master/specs/edu.html | 练习用虚拟卡 |
| OSDev PCIe | https://wiki.osdev.org/PCI_Express | 入门概念 |
| Linux 源码在线交叉引用 | https://elixir.bootlin.com/linux/latest/source | 查函数/调用链 |

## 正版书（想系统读时）

- 《PCI Express 体系结构导读》王齐（中文首选，机械工业出版社）
- *PCI Express Technology 3.0* / *PCI Express System Architecture*（MindShare，https://www.mindshare.com/books/）

## 内核源码（GPL-2.0，本地快照在 `pcie-materials/kernel-src/`）

建议精读顺序：`probe.c` → `pci-driver.c` → `msi/api.c` → `pcie/aer.c` → `nvme/host/pci.c`。
