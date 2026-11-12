// SPDX-License-Identifier: GPL-2.0
/*
 * week3-edu-irq.c —— 第 3 周：中断（从轮询到通知）
 *
 * 本周目标：
 *   1. 申请 MSI 中断（设备不支持就退回 INTx），注册中断处理函数
 *   2. 在 handler 里读 IRQ_STATUS、写 IRQ_ACK 清中断（不 ack 会中断风暴）
 *   3. 把第 2 周的「轮询等阶乘」改成「等完成量」，由中断唤醒
 *   4. 用 irq_count 观察中断真的发生了
 *
 * 关键知识点（面试高频）：
 *   - 中断上下文不能睡眠 -> 所以用 completion，不能用 msleep
 *   - 电平触发中断必须 ack，否则 CPU 会被反复打断
 *   - 共享中断的 handler 要能迅速判断「是不是我的中断」，不是就返回 IRQ_NONE
 *
 * 编译：make
 * 加载：sudo insmod edu_irq.ko
 * 测试：echo 10 | sudo tee /sys/kernel/debug/edu/factorial
 *       sudo dmesg | tail
 * 卸载：sudo rmmod edu_irq
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/types.h>

#define EDU_VENDOR_ID		0x1234
#define EDU_DEVICE_ID		0x11e8

#define EDU_REG_ID		0x00
#define EDU_REG_LIVENESS	0x04
#define EDU_REG_FACTORIAL	0x08
#define EDU_REG_STATUS		0x20
#define EDU_REG_IRQ_STATUS	0x24
#define EDU_REG_IRQ_RAISE	0x60
#define EDU_REG_IRQ_ACK		0x64

#define EDU_STATUS_COMPUTING	0x01
#define EDU_STATUS_IRQFACT	0x80

#define EDU_FACT_IRQ		0x01	/* 中断状态位：阶乘完成 */
#define EDU_DMA_IRQ		0x100	/* 中断状态位：DMA 完成 */

#define EDU_ID_EXPECTED		0x010000ed
#define EDU_FACT_MAX		12

struct edu_dev {
	struct pci_dev *pdev;
	void __iomem *bar;
	struct dentry *dbg;
	struct completion fact_done;
	int irq;
	u32 last_fact;
	unsigned int irq_count;
};

static irqreturn_t edu_irq_handler(int irq, void *data)
{
	struct edu_dev *edu = data;
	u32 status;

	status = ioread32(edu->bar + EDU_REG_IRQ_STATUS);
	if (!status)
		return IRQ_NONE;	/* 不是我的中断 */

	/* 必须在 handler 里应答，否则中断线一直拉高 */
	iowrite32(status, edu->bar + EDU_REG_IRQ_ACK);
	edu->irq_count++;

	if (status & EDU_FACT_IRQ) {
		edu->last_fact = ioread32(edu->bar + EDU_REG_FACTORIAL);
		complete(&edu->fact_done);
	}

	return IRQ_HANDLED;
}

static int edu_fact_set(void *data, u64 val)
{
	struct edu_dev *edu = data;
	unsigned long left;

	if (val > EDU_FACT_MAX)
		return -EINVAL;

	if (ioread32(edu->bar + EDU_REG_STATUS) & EDU_STATUS_COMPUTING)
		return -EBUSY;

	reinit_completion(&edu->fact_done);
	iowrite32((u32)val, edu->bar + EDU_REG_FACTORIAL);

	/* 这次不轮询：睡眠等待中断唤醒 completion（可在进程上下文睡眠） */
	left = wait_for_completion_timeout(&edu->fact_done,
					    msecs_to_jiffies(1000));
	if (!left) {
		dev_err(&edu->pdev->dev, "等阶乘中断超时（irq_count=%u）\n",
			edu->irq_count);
		return -ETIMEDOUT;
	}

	dev_info(&edu->pdev->dev, "第 %u 次中断：%u! = %u\n",
		 edu->irq_count, (u32)val, edu->last_fact);
	return 0;
}

static int edu_fact_get(void *data, u64 *val)
{
	struct edu_dev *edu = data;

	*val = edu->last_fact;
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(edu_fact_fops, edu_fact_get, edu_fact_set, "%llu\n");

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

	pci_set_master(pdev);

	ret = pci_request_regions(pdev, "edu_irq");
	if (ret)
		goto err_disable;

	edu->bar = pci_iomap(pdev, 0, 0);
	if (!edu->bar) {
		ret = -ENOMEM;
		goto err_regions;
	}
	edu->pdev = pdev;
	pci_set_drvdata(pdev, edu);
	init_completion(&edu->fact_done);

	/* 申请中断：优先 MSI，失败退回传统 INTx */
	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
	if (ret < 0) {
		dev_err(&pdev->dev, "申请中断向量失败: %d\n", ret);
		goto err_iomap;
	}
	edu->irq = pci_irq_vector(pdev, 0);

	ret = request_irq(edu->irq, edu_irq_handler, IRQF_SHARED, "edu_irq", edu);
	if (ret) {
		dev_err(&pdev->dev, "request_irq 失败: %d\n", ret);
		goto err_vectors;
	}

	/* 打开「算完阶乘发中断」开关：status 寄存器 0x80 位 */
	iowrite32(EDU_STATUS_IRQFACT, edu->bar + EDU_REG_STATUS);

	edu->dbg = debugfs_create_dir("edu", NULL);
	debugfs_create_file("factorial", 0644, edu->dbg, edu, &edu_fact_fops);

	dev_info(&pdev->dev, "probe 完成：irq=%d（MSI 或 INTx），试试写 debugfs\n",
		 edu->irq);
	return 0;

err_vectors:
	pci_free_irq_vectors(pdev);
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

	/* 先关中断源，再 free_irq，避免卸载过程中还有中断进来 */
	iowrite32(0, edu->bar + EDU_REG_STATUS);
	debugfs_remove_recursive(edu->dbg);
	free_irq(edu->irq, edu);
	pci_free_irq_vectors(pdev);
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
	.name		= "edu_irq",
	.id_table	= edu_ids,
	.probe		= edu_probe,
	.remove		= edu_remove,
};
module_pci_driver(edu_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("driver-prep");
MODULE_DESCRIPTION("Week 3: MSI/INTx interrupt handling for QEMU edu");
