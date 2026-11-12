// SPDX-License-Identifier: GPL-2.0
/*
 * week2-edu-probe.c —— 第 2 周：给 QEMU 的 edu 教学卡写 PCI 驱动
 *
 * 这块卡是 QEMU 内置的「教学设备」（PCI ID 1234:11e8），专门用来练驱动：
 *   - BAR0 是 1MB MMIO 窗口，里面是一排寄存器
 *   - 0x00 读 ID；0x04 liveness（写入值按位取反）；0x08 阶乘寄存器
 *   - 0x80/0x88/0x90/0x98 是 DMA 寄存器（第 4 周用）
 *   - 0x60/0x64 中断触发/应答（第 3 周用）
 *
 * 本周你要做的：
 *   1. probe/remove 回调，用 PCI ID 匹配设备
 *   2. pci_iomap 映射 BAR0，验证能读到设备
 *   3. 做 liveness 与阶乘两个自检
 *   4. 用 debugfs 暴露 /sys/kernel/debug/edu/factorial，从 shell 触发
 *
 * 编译：make
 * 加载：sudo insmod edu_probe.ko
 * 测试：echo 10 | sudo tee /sys/kernel/debug/edu/factorial；cat 它
 * 卸载：sudo rmmod edu_probe
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/types.h>

#define EDU_VENDOR_ID		0x1234
#define EDU_DEVICE_ID		0x11e8

/* BAR0 寄存器偏移（单位：字节），来自 QEMU docs/specs/edu.html */
#define EDU_REG_ID		0x00
#define EDU_REG_LIVENESS	0x04
#define EDU_REG_FACTORIAL	0x08
#define EDU_REG_STATUS		0x20
#define EDU_REG_IRQ_STATUS	0x24
#define EDU_REG_IRQ_RAISE	0x60
#define EDU_REG_IRQ_ACK		0x64

/* status 寄存器位 */
#define EDU_STATUS_COMPUTING	0x01	/* 只读：正在算阶乘 */
#define EDU_STATUS_IRQFACT	0x80	/* 写完：算完阶乘后发中断 */

#define EDU_ID_EXPECTED		0x010000ed	/* 0xRRrr00edu：主版本 01，次版本 00 */
#define EDU_FACT_MAX		12

struct edu_dev {
	struct pci_dev *pdev;
	void __iomem *bar;
	struct dentry *dbg;
	u32 last_fact;
};

/* 从 shell 写这个文件：先写阶乘参数，再轮询等待设备算完 */
static int edu_fact_set(void *data, u64 val)
{
	struct edu_dev *edu = data;
	int i;

	if (val > EDU_FACT_MAX)
		return -EINVAL;

	if (ioread32(edu->bar + EDU_REG_STATUS) & EDU_STATUS_COMPUTING)
		return -EBUSY;

	iowrite32((u32)val, edu->bar + EDU_REG_FACTORIAL);

	/*
	 * 设备在后台线程里计算，而且会故意延迟，
	 * 所以驱动必须轮询 status 的 COMPUTING 位，不能想当然立刻读结果。
	 */
	for (i = 0; i < 200; i++) {
		if (!(ioread32(edu->bar + EDU_REG_STATUS) & EDU_STATUS_COMPUTING))
			break;
		msleep(5);
	}
	if (i == 200)
		return -ETIMEDOUT;

	edu->last_fact = ioread32(edu->bar + EDU_REG_FACTORIAL);
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
	u32 val, fact;
	int ret, i;

	edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
	if (!edu)
		return -ENOMEM;

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	pci_set_master(pdev);

	ret = pci_request_regions(pdev, "edu_probe");
	if (ret)
		goto err_disable;

	edu->bar = pci_iomap(pdev, 0, 0);
	if (!edu->bar) {
		ret = -ENOMEM;
		goto err_regions;
	}
	edu->pdev = pdev;
	pci_set_drvdata(pdev, edu);

	/* 1) ID 寄存器 */
	val = ioread32(edu->bar + EDU_REG_ID);
	if (val == EDU_ID_EXPECTED)
		dev_info(&pdev->dev, "ID=0x%08x 正确（0xRRrr00edu）\n", val);
	else
		dev_warn(&pdev->dev, "ID=0x%08x，期望 0x%08x\n",
			 val, EDU_ID_EXPECTED);

	/* 2) liveness：读回值是写入值的按位取反（规范原文 "simple value inversion"） */
	iowrite32(0xdeadbeef, edu->bar + EDU_REG_LIVENESS);
	val = ioread32(edu->bar + EDU_REG_LIVENESS);
	if (val == (u32)~0xdeadbeef)
		dev_info(&pdev->dev, "liveness 通过：写 0xdeadbeef 读到 0x%08x\n", val);
	else
		dev_warn(&pdev->dev, "liveness 异常：读完 0x%08x，期望 0x%08x\n",
			 val, (u32)~0xdeadbeef);

	/* 3) 阶乘：写 5 应读回 120 */
	iowrite32(5, edu->bar + EDU_REG_FACTORIAL);
	for (i = 0; i < 200; i++) {
		if (!(ioread32(edu->bar + EDU_REG_STATUS) & EDU_STATUS_COMPUTING))
			break;
		msleep(5);
	}
	fact = ioread32(edu->bar + EDU_REG_FACTORIAL);
	if (fact == 120)
		dev_info(&pdev->dev, "factorial 通过：5! = %u\n", fact);
	else
		dev_warn(&pdev->dev, "factorial 异常：5! 读到 %u\n", fact);

	/* 4) debugfs 接口：/sys/kernel/debug/edu/factorial */
	edu->dbg = debugfs_create_dir("edu", NULL);
	debugfs_create_file("factorial", 0644, edu->dbg, edu, &edu_fact_fops);

	dev_info(&pdev->dev, "probe 完成，试试：echo 10 | sudo tee /sys/kernel/debug/edu/factorial\n");
	return 0;

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
	pci_iounmap(pdev, edu->bar);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	dev_info(&pdev->dev, "removed\n");
}

static const struct pci_device_id edu_ids[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, edu_ids);

static struct pci_driver edu_driver = {
	.name		= "edu_probe",
	.id_table	= edu_ids,
	.probe		= edu_probe,
	.remove		= edu_remove,
};
module_pci_driver(edu_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("driver-prep");
MODULE_DESCRIPTION("Week 2: PCI probe + BAR mapping + MMIO registers for QEMU edu");
