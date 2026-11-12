// SPDX-License-Identifier: GPL-2.0
/*
 * week4-edu-dma.c —— 第 4 周：DMA（驱动岗真正的分水岭）
 *
 * 本周目标：
 *   1. 用 dma_set_mask_and_coherent() 告诉内核这块卡的地址位宽
 *   2. 用 dma_alloc_coherent() 分配「设备能直接访问」的一致性内存
 *   3. 写 DMA 寄存器：src(0x80) / dst(0x88) / cnt(0x90) / cmd(0x98)
 *      cmd 位：0x1=启动  0x2=方向(0:RAM->卡, 1:卡->RAM)  0x4=完成发中断
 *   4. 做 RAM -> 设备内部 buffer -> RAM 的往返，用 memcmp 证明数据真的
 *      走过卡上的 DMA 引擎（卡内部 buffer 的"地址"是 0x40000，大小 4096）
 *
 * 面试高频：
 *   - 一致性 DMA 和流式 DMA 的区别？dma_alloc_coherent vs dma_map_single
 *   - 为什么 DMA 地址要用 dma_alloc 返回的 handle，而不是 virt_to_phys？
 *   - 设备地址位宽不够（比如只有 28 位）时驱动要做什么？
 *   - DMA 完成为什么用中断而不是轮询？
 *
 * 注意：QEMU 的 edu 默认只支持 28 位 DMA 地址（会把高地址 clamp 掉），
 * 所以启动脚本里用了 -device edu,dma_mask=0xffffffff，驱动里再声明 32 位。
 *
 * 编译：make
 * 加载：sudo insmod edu_dma.ko
 * 测试：echo 1 | sudo tee /sys/kernel/debug/edu/dma
 *       sudo dmesg | tail
 * 卸载：sudo rmmod edu_dma
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/types.h>

#define EDU_VENDOR_ID		0x1234
#define EDU_DEVICE_ID		0x11e8

#define EDU_REG_DMA_SRC		0x80
#define EDU_REG_DMA_DST		0x88
#define EDU_REG_DMA_CNT		0x90
#define EDU_REG_DMA_CMD		0x98

#define EDU_REG_STATUS		0x20
#define EDU_REG_IRQ_STATUS	0x24
#define EDU_REG_IRQ_ACK		0x64

#define EDU_DMA_START		0x40000	/* 设备内部 buffer 的地址 */
#define EDU_DMA_BUF_SIZE	4096	/* 设备内部 buffer 大小 */
#define EDU_DMA_RUN		0x01
#define EDU_DMA_DIR		0x02	/* 0: RAM -> 设备；1: 设备 -> RAM */
#define EDU_DMA_IRQ_EN		0x04
#define EDU_DMA_IRQ		0x100	/* 中断状态位：DMA 完成 */
#define EDU_DMA_LEN		64	/* 本次传输长度 */

struct edu_dev {
	struct pci_dev *pdev;
	void __iomem *bar;
	struct dentry *dbg;
	struct completion dma_done;
	int irq;
	void *src;
	dma_addr_t src_dma;
	void *dst;
	dma_addr_t dst_dma;
	unsigned int irq_count;
	int last_ret;
};

static irqreturn_t edu_irq_handler(int irq, void *data)
{
	struct edu_dev *edu = data;
	u32 status;

	status = ioread32(edu->bar + EDU_REG_IRQ_STATUS);
	if (!status)
		return IRQ_NONE;

	iowrite32(status, edu->bar + EDU_REG_IRQ_ACK);
	edu->irq_count++;

	if (status & EDU_DMA_IRQ)
		complete(&edu->dma_done);

	return IRQ_HANDLED;
}

static int edu_dma_set(void *data, u64 val)
{
	struct edu_dev *edu = data;
	struct device *dev = &edu->pdev->dev;
	u8 *src = edu->src, *dst = edu->dst;
	unsigned long left;
	int i, ret = 0;

	/* 1) 准备两块 buffer 的数据 */
	for (i = 0; i < EDU_DMA_LEN; i++)
		src[i] = (u8)(0xa0 + i);
	memset(dst, 0, EDU_DMA_LEN);

	/* 2) RAM -> 设备内部 buffer */
	reinit_completion(&edu->dma_done);
	iowrite32((u32)edu->src_dma, edu->bar + EDU_REG_DMA_SRC);
	iowrite32(EDU_DMA_START, edu->bar + EDU_REG_DMA_DST);
	iowrite32(EDU_DMA_LEN, edu->bar + EDU_REG_DMA_CNT);
	iowrite32(EDU_DMA_RUN | EDU_DMA_IRQ_EN, edu->bar + EDU_REG_DMA_CMD);

	left = wait_for_completion_timeout(&edu->dma_done,
					    msecs_to_jiffies(1000));
	if (!left) {
		dev_err(dev, "DMA(RAM -> EDU) 超时\n");
		ret = -ETIMEDOUT;
		goto out;
	}

	/* 3) 设备内部 buffer -> RAM（方向位 0x2） */
	reinit_completion(&edu->dma_done);
	iowrite32(EDU_DMA_START, edu->bar + EDU_REG_DMA_SRC);
	iowrite32((u32)edu->dst_dma, edu->bar + EDU_REG_DMA_DST);
	iowrite32(EDU_DMA_LEN, edu->bar + EDU_REG_DMA_CNT);
	iowrite32(EDU_DMA_RUN | EDU_DMA_DIR | EDU_DMA_IRQ_EN,
		  edu->bar + EDU_REG_DMA_CMD);

	left = wait_for_completion_timeout(&edu->dma_done,
					    msecs_to_jiffies(1000));
	if (!left) {
		dev_err(dev, "DMA(EDU -> RAM) 超时\n");
		ret = -ETIMEDOUT;
		goto out;
	}

	/* 4) 数据校验：往返后必须一字不差 */
	if (memcmp(src, dst, EDU_DMA_LEN)) {
		dev_err(dev, "DMA 往返数据不一致！\n");
		ret = -EIO;
		goto out;
	}

	dev_info(dev, "DMA 往返成功：%d 字节一致，累计中断 %u 次\n",
		 EDU_DMA_LEN, edu->irq_count);

out:
	edu->last_ret = ret;
	return ret;
}

static int edu_dma_get(void *data, u64 *val)
{
	struct edu_dev *edu = data;

	*val = edu->last_ret;
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(edu_dma_fops, edu_dma_get, edu_dma_set, "%llu\n");

static int edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu_dev *edu;
	int ret;

	edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
	if (!edu)
		return -ENOMEM;

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	pci_set_master(pdev);	/* 打开 bus master，DMA 必需 */

	ret = pci_request_regions(pdev, "edu_dma");
	if (ret)
		goto err_disable;

	edu->bar = pci_iomap(pdev, 0, 0);
	if (!edu->bar) {
		ret = -ENOMEM;
		goto err_regions;
	}
	edu->pdev = pdev;
	pci_set_drvdata(pdev, edu);
	init_completion(&edu->dma_done);

	/* 告诉内核：这块卡只能访问 32 位地址 */
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(&pdev->dev, "dma_set_mask 失败: %d\n", ret);
		goto err_iomap;
	}

	/* 一致性 DMA 内存：返回的 dma_addr_t 才是给设备用的地址 */
	edu->src = dma_alloc_coherent(&pdev->dev, EDU_DMA_BUF_SIZE,
				      &edu->src_dma, GFP_KERNEL);
	edu->dst = dma_alloc_coherent(&pdev->dev, EDU_DMA_BUF_SIZE,
				      &edu->dst_dma, GFP_KERNEL);
	if (!edu->src || !edu->dst) {
		dev_err(&pdev->dev, "dma_alloc_coherent 失败\n");
		ret = -ENOMEM;
		goto err_free_dma;
	}
	dev_info(&pdev->dev, "DMA buffer: src=%pad dst=%pad\n",
		 &edu->src_dma, &edu->dst_dma);

	/* 中断：DMA 完成通知 */
	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
	if (ret < 0)
		goto err_free_dma;
	edu->irq = pci_irq_vector(pdev, 0);

	ret = request_irq(edu->irq, edu_irq_handler, IRQF_SHARED, "edu_dma", edu);
	if (ret)
		goto err_vectors;

	edu->dbg = debugfs_create_dir("edu", NULL);
	debugfs_create_file("dma", 0644, edu->dbg, edu, &edu_dma_fops);

	dev_info(&pdev->dev, "probe 完成：irq=%d，试试 echo 1 > /sys/kernel/debug/edu/dma\n",
		 edu->irq);
	return 0;

err_vectors:
	pci_free_irq_vectors(pdev);
err_free_dma:
	if (edu->src)
		dma_free_coherent(&pdev->dev, EDU_DMA_BUF_SIZE, edu->src,
				  edu->src_dma);
	if (edu->dst)
		dma_free_coherent(&pdev->dev, EDU_DMA_BUF_SIZE, edu->dst,
				  edu->dst_dma);
err_iomap:
	pci_iounmap(pdev, edu->bar);
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

static void edu_remove(struct pci_dev *pdev)
{
	struct edu_dev *edu = pci_get_drvdata(pdev);

	debugfs_remove_recursive(edu->dbg);
	free_irq(edu->irq, edu);
	pci_free_irq_vectors(pdev);
	dma_free_coherent(&pdev->dev, EDU_DMA_BUF_SIZE, edu->src, edu->src_dma);
	dma_free_coherent(&pdev->dev, EDU_DMA_BUF_SIZE, edu->dst, edu->dst_dma);
	pci_iounmap(pdev, edu->bar);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	dev_info(&pdev->dev, "removed（累计中断 %u 次）\n", edu->irq_count);
}

static const struct pci_device_id edu_ids[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, edu_ids);

static struct pci_driver edu_driver = {
	.name		= "edu_dma",
	.id_table	= edu_ids,
	.probe		= edu_probe,
	.remove		= edu_remove,
};
module_pci_driver(edu_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("driver-prep");
MODULE_DESCRIPTION("Week 4: coherent DMA round-trip on QEMU edu");
