# 第 5 周：字符设备 + UAPI（ioctl / mmap / 多进程）

> **目标**：把 week2~4 的"能跑但只能 debugfs 操作"的 PCI 驱动，升级成**用户态可正常使用**的加速卡驱动。
> 这一周对应技能树：**L1.2（字符设备与 UAPI）、L1.3（内核内存）、L1.4（并发）、L3.5（多进程）**。
> **它是"驱动实习生"和"驱动工程师"的分水岭**：ioctl ABI、mmap、多进程资源生命周期——面试必问。

---

## 一、本周要交付什么（验收标准）

全部完成后，你应该能：

| # | 验收项 | 判断方法 |
|---|---|---|
| 1 | 驱动加载后自动出现 `/dev/edu5` | `ls -l /dev/edu5`（若无，用 `mknod` 手动建） |
| 2 | `ioctl(EDU5_IOC_GET_INFO)` 返回设备信息 | 用户态程序打印 BAR 大小/IRQ/中断计数 |
| 3 | `ioctl(EDU5_IOC_FACTORIAL)` 提交阶乘并等待中断完成 | 输入 10 返回 3628800 |
| 4 | `ioctl(EDU5_IOC_DMA_TEST)` 完成一次 DMA 往返并校验 | 返回 0，dmesg 有成功日志 |
| 5 | `mmap` 能映射设备 buffer 并在用户态读写 | 写进去再读出来一致 |
| 6 | **两个进程同时打开设备互不干扰** | 并发跑测试，无崩溃、计数正确 |
| 7 | 进程被 `kill -9` 后资源正确回收 | 再打开不受影响，`cat /proc/meminfo` 无泄漏迹象 |

---

## 二、接口设计（UAPI 契约，先定后写）

`edu_uapi.h` 是**驱动和用户态共享**的头文件（真实项目里放 `include/uapi/linux/xxx.h`）。

| ioctl | 方向 | 参数 | 语义 |
|---|---|---|---|
| `EDU5_IOC_GET_INFO` | 读 | `struct edu5_info *` | 返回 BAR 大小、IRQ、中断计数、可 mmap 大小 |
| `EDU5_IOC_FACTORIAL` | 读写 | `struct edu5_fact_arg *` | 入参 n（0~12），出参 n! |
| `EDU5_IOC_DMA_TEST` | 写 | `__u32` | 跑一次 DMA 往返，0=成功 |
| `EDU5_IOC_GET_MAP_SIZE` | 读 | `__u32 *` | 返回 mmap 可用大小（PAGE_SIZE） |

**错误码约定**：`-EINVAL` 参数非法；`-EBUSY` 设备忙；`-ETIMEDOUT` 等中断超时；`-ENOTTY` 未知命令。

**设计要点（面试会问）**：
- ioctl 号段用 `_IOW/_IOR/_IOWR` + magic，避免和其他驱动冲突；
- 结构体里**必须预留字段**（`__u32 reserved[4]`），以后加功能不破坏 ABI；
- ioctl 里**不能直接解引用用户指针**，必须 `copy_from_user/copy_to_user`。

---

## 三、实现步骤（按顺序做）

### Step 1：字符设备注册（30 分钟）
- `alloc_chrdev_region()` 拿设备号
- `cdev_init()` + `cdev_add()` 注册 `file_operations`
- `class_create()` + `device_create()` 创建 `/dev/edu5`
- 卸载时逆序销毁（`device_destroy` → `class_destroy` → `cdev_del` → `unregister_chrdev_region`）

### Step 2：file_operations 骨架
- `open`：`container_of(inode->i_cdev, struct edu5_dev, cdev)` 拿到设备
- 每个打开分配一个 `struct edu5_file`，挂到 `file->private_data`
- `release`：释放 per-file 数据，减全局计数
- 未实现的回调返回 `-EINVAL` 或 `-ENOTTY`

### Step 3：ioctl 实现（核心）
- `GET_INFO`：填 `pci_resource_len(pdev, 0)`、`edu->irq`、`edu->irq_count`，`copy_to_user`
- `FACTORIAL`：`copy_from_user` 取 n → 加锁 → 写阶乘寄存器 → `wait_for_completion_timeout` → 读结果 → `copy_to_user`
- `DMA_TEST`：复用 week4 的 DMA 往返逻辑，加锁串行化
- 错误路径都要返回负 errno

### Step 4：mmap（容易踩坑）
- 分配一页内核 buffer（`__get_free_page` 或 `dma_alloc_coherent`）
- `remap_pfn_range(vma, vma->vm_start, virt_to_phys(buf) >> PAGE_SHIFT, PAGE_SIZE, vma->vm_page_prot)`
- 检查 `vma->vm_pgoff`、`size`；设置 `VM_DONTEXPAND | VM_DONTDUMP`
- **思考**：这种 mmap 有什么安全问题？`dma_alloc_coherent` 的 buffer mmap 给用户态，cache 一致性怎么保证？

### Step 5：多进程正确性
- 全局资源（设备、IRQ、DMA buffer）用锁保护
- per-file 资源（打开计数、私有 buffer）各自独立
- `release` 里确保无论进程怎么退出（正常/ kill -9）都能回收

### Step 6（加分）：poll / 异步
- 加 `waitqueue`，DMA 完成时 `wake_up`
- `poll` 返回 `EPOLLIN`，用户态 `epoll` 等待完成事件

---

## 四、用户态测试程序

`app/test_edu5.c` 依次做：
1. `open("/dev/edu5")`
2. `EDU5_IOC_GET_INFO` 打印信息
3. `EDU5_IOC_FACTORIAL` 算 10!
4. `mmap` + 写/读校验
5. `EDU5_IOC_DMA_TEST`
6. `close`

编译运行（在 guest 里）：

```bash
cd ~/driver-prep/week5-chardev-uapi/app
make
sudo ./test_edu5
```

> 一键端到端测试（在 WSL 里，自动完成"编译 module → 部署 → guest 内加载 → 跑 App"）：
> ```bash
> bash scripts/test-week5.sh
> ```
> 注意：`/dev/edu5` 默认权限是 `crw------- root`，所以 App 要用 `sudo` 跑；生产环境应加 udev 规则给普通用户权限。

多进程测试：
```bash
# 两个终端同时跑
sudo ./test_edu5 &
sudo ./test_edu5
```

---

## 五、调试手段

| 现象 | 排查 |
|---|---|
| 没有 `/dev/edu5` | `cat /proc/devices` 看主设备号；`udev` 没建就 `sudo mknod /dev/edu5 c <major> 0` |
| ioctl 返回 `-ENOTTY` | 命令号不匹配：检查 `_IOW/_IOR` 方向与结构体大小 |
| 等中断超时 | 确认 interrupt 已注册、`irq_count` 在涨、设备在发中断 |
| `mmap` 段错误 | `remap_pfn_range` 的物理地址/大小/vma 标志检查 |
| 进程退出后打不开 | 引用计数泄漏：`release` 里是否减计数、锁是否释放 |

---

## 六、思考题（面试原题）

1. 为什么 ioctl 参数要用 `copy_from_user`，不能直接 `*arg`？
2. `mmap` 的 `remap_pfn_range` 和 `vm_insert_page` 有什么区别？各用在哪？
3. 两个进程同时 `open`，驱动里哪些数据是共享的、哪些是私有的？
4. 进程 `kill -9` 时内核会调用 `release` 吗？如果进程在内核态死循环呢？
5. 设备内存 mmap 给用户态，用户态读写与 DMA 同时发生会有什么问题？
6. 为什么 ioctl 结构体要预留字段？内核 ABI 兼容性是怎么保证的？
7. `poll` 和中断/等待队列怎么配合实现"异步完成通知"？
8. 如果要在 ioctl 里睡眠等 DMA，能不能用自旋锁？为什么？

---

## 七、提交要求

完成后：

```bash
cd /mnt/d/2FPGA/driver-prep
git add -A
git commit -m "week5: implement char device UAPI (ioctl/mmap/multiprocess)"
git push
```

然后在 `docs/skill-map.md` 第 13 节把第 5 周对应项打勾。

---

## 八、安全

只在 `driver-prep` 的 QEMU 虚拟机里做实验；工控机/真实采集卡上**不要加载**这个驱动。
