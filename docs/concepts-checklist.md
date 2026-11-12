# 自测题清单：驱动/内核方向

> 用法：每周做完实验后，**合上代码**，对着问题口述答案。答不出来的，就是下一遍要弄懂的地方。
> 这些问题就是太初驱动岗面试的高频题，能用大白话讲清 = 真懂。

---

## 第 1 周：内核模块基础

1. `insmod` 到你的 `hello_init` 被调用，中间经过了什么？
2. `printk` 和 `printf` 的区别？日志等级怎么看？
3. `module_init/module_exit` 展开后是什么？
4. `rmmod` 时如果还有进程在用这个模块会怎样？引用计数谁维护？
5. 内核模块的 `.ko` 里有哪些段？`modinfo` 显示的信息从哪来？

---

## 第 2 周：PCI 驱动与 MMIO

1. 内核是怎么"找到"你的驱动的？`MODULE_DEVICE_TABLE` 和 `modalias` 是什么关系？
2. `probe` 里为什么一般要 `pci_enable_device` + `pci_request_regions` + `pci_iomap`？
   顺序能换吗？各步失败应该怎么回滚？
3. BAR 是什么？为什么设备寄存器不直接映射到物理地址？
4. `ioread32` / `readl` 和普通指针解引用有什么区别？`__iomem` 修饰的作用？
5. `devm_kzalloc` 比 `kzalloc` 好在哪？`remove` 里还要不要手动释放？
6. debugfs 和 procfs、sysfs 各自适合放什么？为什么内核不推荐在 sysfs 里做复杂控制？
7. 你的驱动 `remove` 后重新 `insmod`，为什么没报错？

---

## 第 3 周：中断与并发

1. 中断处理函数里为什么不能睡眠？如果必须做耗时操作怎么办？
2. MSI / MSI-X / INTx 的区别？为什么现代设备偏爱 MSI-X？
3. 中断处理函数为什么要写 `IRQ_ACK`？不写会发生什么？
4. `IRQF_SHARED` 的含义？共享中断的 handler 为什么必须能判断"是不是我的中断"？
5. 为什么 handler 用 `complete()` 而不是 `wake_up` + 自己维护标志位？
6. `request_irq` 的 `dev_id` 有什么用？`/proc/interrupts` 里的名字从哪来？
7. 卸载驱动时为什么先关中断源再 `free_irq`？顺序反了会怎样？
8. 电平触发和边沿触发中断的区别？（按 QEMU edu 实际行为回答）

---

## 第 4 周：DMA 与内存

1. DMA 是什么？为什么不让 CPU 搬数据？
2. `dma_alloc_coherent` 和 `dma_map_single` 的区别？各自用在什么场景？
3. 为什么 DMA 地址要用 `dma_alloc_coherent` 返回的 handle，而不是 `virt_to_phys(ptr)`？
4. 一致性 DMA（coherent）和流式 DMA（streaming）的 cache 一致性分别怎么处理？
5. `pci_set_master()` 是干什么的？漏调用会怎样？
6. 设备只支持 28 位地址时，驱动要怎么配合？内核的 bounce buffer（SWIOTLB）在什么情况下介入？
7. DMA 完成为什么用中断通知而不是轮询？高吞吐场景为什么又会用轮询（如 DPDK）？
8. 一次 DMA 传输的完整流程，从用户请求到数据回来，经过哪些层？

---

## 附加：岗位相关的进阶题（4 周后再看）

1. 多进程同时 `open` 同一张加速卡，驱动要维护什么？进程异常退出时怎么办？
2. 怎么把设备内存 `mmap` 给用户态？`remap_pfn_range` / `vm_insert_page` 的区别？
3. 一个 GPU/加速卡驱动里的"命令队列 + 上下文 + 调度器"大概怎么设计？
4. SR-IOV 的 PF/VF 怎么工作？算力切分（一张卡切成多份）在驱动层要做什么？
5. 内核 panic / oops 了怎么定位？`kdump` + `crash` 大概流程？
6. 内核模块的并发问题怎么排查？KASAN/KCSAN/锁依赖检查器各管什么？

---

## 自评标准

- **能讲清 80% 以上** → 驱动岗校招技术面基本能过，可以投简历了。
- **能讲清 50%** → 项目做过但没内化，把对应周的代码重写一遍再来。
- **不到 50%** → 别急着投，先把 week2/week3 重做一遍，重点是"每一步为什么"。
