// SPDX-License-Identifier: GPL-2.0
/*
 * week5-chardev-uapi/edu_chardev.c —— 第 5 周：字符设备 + UAPI
 *
 * 目标：把 week2~4 的驱动升级成用户态可正常使用的字符设备：
 *   1. /dev/edu5 字符设备（cdev + class + device）
 *   2. file_operations：open/release/ioctl/mmap
 *   3. ioctl：GET_INFO / FACTORIAL / DMA_TEST / GET_MAP_SIZE
 *   4. mmap：把一页内核 buffer 映射给用户态
 *   5. 多进程：per-file 私有数据 + 全局引用计数
 *
 * 编译（在 WSL 里）：bash scripts/build.sh week5-chardev-uapi
 * 加载（在 guest 里）：sudo insmod edu_chardev.ko
 * 测试：见 app/test_edu5.c
 *
 * 注意：本文件是"可编译骨架"，标了 TODO 的地方请你补完。
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/atomic.h>

#include "edu_uapi.h"

/* ===== QEMU edu 卡寄存器（与 week2~4 相同） ===== */
#define EDU_VENDOR_ID		0x1234
#define EDU_DEVICE_ID		0x11e8

#define EDU_REG_ID		0x00
#define EDU_REG_FACTORIAL	0x08
#define EDU_REG_STATUS		0x20
#define EDU_REG_IRQ_STATUS	0x24
#define EDU_REG_IRQ_ACK		0x64
#define EDU_REG_DMA_SRC		0x80
#define EDU_REG_DMA_DST		0x88
#define EDU_REG_DMA_CNT		0x90
#define EDU_REG_DMA_CMD		0x98

#define EDU_STATUS_IRQFACT	0x80
#define EDU_FACT_IRQ		0x01
#define EDU_DMA_IRQ		0x100

#define EDU_DMA_START		0x40000
#define EDU_DMA_BUF_SIZE	4096
#define EDU_DMA_RUN		0x01
#define EDU_DMA_DIR		0x02
#define EDU_DMA_IRQ_EN		0x04
#define EDU_DMA_LEN		64

struct edu5_dev {
	struct pci_dev *pdev;
	void __iomem *bar;
	int irq;

	struct completion fact_done;
	struct completion dma_done;
	u32 last_fact;
	u32 irq_count;

	/* 字符设备 */
	dev_t devno;
	struct cdev cdev;
	struct class *class;
	struct device *dev;

	/* DMA 一致性内存 */
	void *dma_src;
	dma_addr_t dma_src_addr;
	void *dma_dst;
	dma_addr_t dma_dst_addr;

	/* 供用户态 mmap 的一页 */
	void *mmap_buf;
	phys_addr_t mmap_phys;

	struct mutex lock;	/* 串行化硬件操作 */
	atomic_t open_count;
};

/* 每次 open 的私有数据（多进程隔离的关键） */
struct edu5_file {
	struct edu5_dev *dev;
	u32 magic;
};
#define EDU5_FILE_MAGIC	0x45445535

static struct edu5_dev *edu5;

/* ================= 中断处理 ================= */

static irqreturn_t edu5_irq_handler(int irq, void *data)
{
	struct edu5_dev *edu = data;
	u32 status;

	status = ioread32(edu->bar + EDU_REG_IRQ_STATUS);
	if (!status)
		return IRQ_NONE;	/* 不是我的中断 */

	iowrite32(status, edu->bar + EDU_REG_IRQ_ACK);
	edu->irq_count++;

	if (status & EDU_FACT_IRQ) {
		edu->last_fact = ioread32(edu->bar + EDU_REG_FACTORIAL);
		complete(&edu->fact_done);
	}
	if (status & EDU_DMA_IRQ)
		complete(&edu->dma_done);

	return IRQ_HANDLED;
}

/* ================= file_operations ================= */

static int edu5_open(struct inode *inode, struct file *file)
{
	struct edu5_dev *edu = container_of(inode->i_cdev, struct edu5_dev, cdev);
	struct edu5_file *f;

	f = kzalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;

	f->dev = edu;
	f->magic = EDU5_FILE_MAGIC;
	file->private_data = f;
	atomic_inc(&edu->open_count);

	dev_dbg(&edu->pdev->dev, "open: count=%d\n",
		atomic_read(&edu->open_count));
	return 0;
}

static int edu5_release(struct inode *inode, struct file *file)
{
	struct edu5_file *f = file->private_data;
	struct edu5_dev *edu;

	if (!f || f->magic != EDU5_FILE_MAGIC)
		return -EINVAL;

	edu = f->dev;
	f->magic = 0;
	kfree(f);
	file->private_data = NULL;
	atomic_dec(&edu->open_count);

	dev_dbg(&edu->pdev->dev, "release: count=%d\n",
		atomic_read(&edu->open_count));
	return 0;
}

/* ================= 硬件操作（进程上下文，可睡眠） ================= */

static int edu5_factorial(struct edu5_dev *edu, u32 n, u32 *out)
{
	unsigned long left;

	if (n > EDU5_MAX_FACTORIAL)
		return -EINVAL;

	reinit_completion(&edu->fact_done);
	iowrite32(n, edu->bar + EDU_REG_FACTORIAL);

	left = wait_for_completion_timeout(&edu->fact_done,
					    msecs_to_jiffies(1000));
	if (!left)
		return -ETIMEDOUT;

	*out = ioread32(edu->bar + EDU_REG_FACTORIAL);
	return 0;
}

static int edu5_dma_roundtrip(struct edu5_dev *edu)
{
	u8 *src = edu->dma_src;
	u8 *dst = edu->dma_dst;
	unsigned long left;
	int i;

	for (i = 0; i < EDU_DMA_LEN; i++)
		src[i] = (u8)(0xa0 + i);
	memset(dst, 0, EDU_DMA_LEN);

	/* ① RAM -> 设备内部 buffer */
	reinit_completion(&edu->dma_done);
	iowrite32((u32)edu->dma_src_addr, edu->bar + EDU_REG_DMA_SRC);
	iowrite32(EDU_DMA_START, edu->bar + EDU_REG_DMA_DST);
	iowrite32(EDU_DMA_LEN, edu->bar + EDU_REG_DMA_CNT);
	iowrite32(EDU_DMA_RUN | EDU_DMA_IRQ_EN, edu->bar + EDU_REG_DMA_CMD);

	left = wait_for_completion_timeout(&edu->dma_done,
					    msecs_to_jiffies(1000));
	if (!left)
		return -ETIMEDOUT;

	/* ② 设备内部 buffer -> RAM */
	reinit_completion(&edu->dma_done);
	iowrite32(EDU_DMA_START, edu->bar + EDU_REG_DMA_SRC);
	iowrite32((u32)edu->dma_dst_addr, edu->bar + EDU_REG_DMA_DST);
	iowrite32(EDU_DMA_LEN, edu->bar + EDU_REG_DMA_CNT);
	iowrite32(EDU_DMA_RUN | EDU_DMA_DIR | EDU_DMA_IRQ_EN,
		  edu->bar + EDU_REG_DMA_CMD);

	left = wait_for_completion_timeout(&edu->dma_done,
					    msecs_to_jiffies(1000));
	if (!left)
		return -ETIMEDOUT;

	return memcmp(src, dst, EDU_DMA_LEN) ? -EIO : 0;
}

/* ================= ioctl ================= */

static long edu5_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct edu5_file *f = file->private_data;
	struct edu5_dev *edu;
	void __user *uarg = (void __user *)arg;
	int ret = 0;

	if (!f || f->magic != EDU5_FILE_MAGIC)
		return -EINVAL;
	edu = f->dev;

	if (_IOC_TYPE(cmd) != EDU5_IOC_MAGIC || _IOC_NR(cmd) > EDU5_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {
	case EDU5_IOC_GET_INFO: {
		struct edu5_info info;

		memset(&info, 0, sizeof(info));
		info.bar_size = (u32)pci_resource_len(edu->pdev, 0);
		info.irq = (u32)edu->irq;
		info.irq_count = edu->irq_count;
		info.mmap_size = PAGE_SIZE;
		if (copy_to_user(uarg, &info, sizeof(info)))
			return -EFAULT;
		break;
	}
	case EDU5_IOC_FACTORIAL: {
		struct edu5_fact_arg a;
		u32 result = 0;

		if (copy_from_user(&a, uarg, sizeof(a)))
			return -EFAULT;

		mutex_lock(&edu->lock);
		ret = edu5_factorial(edu, a.n, &result);
		mutex_unlock(&edu->lock);
		if (ret)
			break;

		a.result = result;
		if (copy_to_user(uarg, &a, sizeof(a)))
			return -EFAULT;
		break;
	}
	case EDU5_IOC_DMA_TEST: {
		u32 flag;

		if (copy_from_user(&flag, uarg, sizeof(flag)))
			return -EFAULT;

		mutex_lock(&edu->lock);
		ret = edu5_dma_roundtrip(edu);
		mutex_unlock(&edu->lock);
		break;
	}
	case EDU5_IOC_GET_MAP_SIZE: {
		u32 size = PAGE_SIZE;

		if (copy_to_user(uarg, &size, sizeof(size)))
			return -EFAULT;
		break;
	}
	default:
		ret = -ENOTTY;
		break;
	}

	/* TODO(加分项): poll 支持——加 waitqueue，在 DMA 完成时 wake_up */

	return ret;
}

/* ================= mmap ================= */

static int edu5_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct edu5_file *f = file->private_data;
	struct edu5_dev *edu;
	unsigned long size = vma->vm_end - vma->vm_start;

	if (!f || f->magic != EDU5_FILE_MAGIC)
		return -EINVAL;
	edu = f->dev;

	if (vma->vm_pgoff != 0 || size > PAGE_SIZE)
		return -EINVAL;

	/* 6.3+ 内核要求用 vm_flags_set()，不能直接改 vma->vm_flags */
	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);

	if (remap_pfn_range(vma, vma->vm_start,
			    edu->mmap_phys >> PAGE_SHIFT,
			    size, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static const struct file_operations edu5_fops = {
	.owner		= THIS_MODULE,
	.open		= edu5_open,
	.release	= edu5_release,
	.unlocked_ioctl	= edu5_ioctl,
	.mmap		= edu5_mmap,
};

/* ================= PCI probe / remove ================= */

static int edu5_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu5_dev *edu;
	int ret;

	edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
	if (!edu)
		return -ENOMEM;

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	pci_set_master(pdev);

	ret = pci_request_regions(pdev, EDU5_DEV_NAME);
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
	init_completion(&edu->dma_done);
	mutex_init(&edu->lock);
	atomic_set(&edu->open_count, 0);

	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
		goto err_iomap;

	edu->dma_src = dma_alloc_coherent(&pdev->dev, EDU_DMA_BUF_SIZE,
					  &edu->dma_src_addr, GFP_KERNEL);
	edu->dma_dst = dma_alloc_coherent(&pdev->dev, EDU_DMA_BUF_SIZE,
					  &edu->dma_dst_addr, GFP_KERNEL);
	if (!edu->dma_src || !edu->dma_dst) {
		ret = -ENOMEM;
		goto err_dma;
	}

	edu->mmap_buf = (void *)__get_free_page(GFP_KERNEL);
	if (!edu->mmap_buf) {
		ret = -ENOMEM;
		goto err_dma;
	}
	edu->mmap_phys = virt_to_phys(edu->mmap_buf);

	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
	if (ret < 0)
		goto err_mmap;
	edu->irq = pci_irq_vector(pdev, 0);

	ret = request_irq(edu->irq, edu5_irq_handler, IRQF_SHARED,
			  EDU5_DEV_NAME, edu);
	if (ret)
		goto err_vectors;

	/* 打开"阶乘完成发中断"开关 */
	iowrite32(EDU_STATUS_IRQFACT, edu->bar + EDU_REG_STATUS);

	ret = alloc_chrdev_region(&edu->devno, 0, 1, EDU5_DEV_NAME);
	if (ret)
		goto err_irq;

	cdev_init(&edu->cdev, &edu5_fops);
	edu->cdev.owner = THIS_MODULE;
	ret = cdev_add(&edu->cdev, edu->devno, 1);
	if (ret)
		goto err_chrdev;

	edu->class = class_create(EDU5_DEV_NAME);
	if (IS_ERR(edu->class)) {
		ret = PTR_ERR(edu->class);
		goto err_cdev;
	}

	edu->dev = device_create(edu->class, &pdev->dev, edu->devno,
				 NULL, EDU5_DEV_NAME);
	if (IS_ERR(edu->dev)) {
		ret = PTR_ERR(edu->dev);
		goto err_class;
	}

	edu5 = edu;
	dev_info(&pdev->dev,
		 "ready: /dev/%s major=%u irq=%d bar0=0x%llx size=%llu\n",
		 EDU5_DEV_NAME, MAJOR(edu->devno), edu->irq,
		 (unsigned long long)pci_resource_start(pdev, 0),
		 (unsigned long long)pci_resource_len(pdev, 0));
	return 0;

err_class:
	class_destroy(edu->class);
err_cdev:
	cdev_del(&edu->cdev);
err_chrdev:
	unregister_chrdev_region(edu->devno, 1);
err_irq:
	free_irq(edu->irq, edu);
err_vectors:
	pci_free_irq_vectors(pdev);
err_mmap:
	free_page((unsigned long)edu->mmap_buf);
err_dma:
	if (edu->dma_src)
		dma_free_coherent(&pdev->dev, EDU_DMA_BUF_SIZE,
				  edu->dma_src, edu->dma_src_addr);
	if (edu->dma_dst)
		dma_free_coherent(&pdev->dev, EDU_DMA_BUF_SIZE,
				  edu->dma_dst, edu->dma_dst_addr);
err_iomap:
	pci_iounmap(pdev, edu->bar);
err_regions:
	pci_release_regions(pdev);
err_disable:
	pci_disable_device(pdev);
	return ret;
}

static void edu5_remove(struct pci_dev *pdev)
{
	struct edu5_dev *edu = pci_get_drvdata(pdev);

	edu5 = NULL;

	device_destroy(edu->class, edu->devno);
	class_destroy(edu->class);
	cdev_del(&edu->cdev);
	unregister_chrdev_region(edu->devno, 1);

	/* 先关中断源，再 free_irq */
	iowrite32(0, edu->bar + EDU_REG_STATUS);
	free_irq(edu->irq, edu);
	pci_free_irq_vectors(pdev);

	free_page((unsigned long)edu->mmap_buf);
	dma_free_coherent(&pdev->dev, EDU_DMA_BUF_SIZE,
			  edu->dma_src, edu->dma_src_addr);
	dma_free_coherent(&pdev->dev, EDU_DMA_BUF_SIZE,
			  edu->dma_dst, edu->dma_dst_addr);

	pci_iounmap(pdev, edu->bar);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	dev_info(&pdev->dev, "removed\n");
}

static const struct pci_device_id edu5_ids[] = {
	{ PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID) },
	{ }
};
MODULE_DEVICE_TABLE(pci, edu5_ids);

static struct pci_driver edu5_driver = {
	.name		= "edu_chardev",
	.id_table	= edu5_ids,
	.probe		= edu5_probe,
	.remove		= edu5_remove,
};
module_pci_driver(edu5_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("driver-prep");
MODULE_DESCRIPTION("Week 5: char device + UAPI (ioctl/mmap) for QEMU edu");
