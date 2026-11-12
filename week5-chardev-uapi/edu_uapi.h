/* SPDX-License-Identifier: GPL-2.0 */
/*
 * week5-chardev-uapi/edu_uapi.h
 *
 * 驱动与用户态共享的 UAPI 定义（真实项目里放 include/uapi/linux/xxx.h）。
 * 要求：
 *   - 只用 __u32/__u64 等固定宽度类型
 *   - 结构体预留字段（ABI 兼容）
 *   - ioctl 号段用 magic + 方向宏
 */
#ifndef _EDU_UAPI_H
#define _EDU_UAPI_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define EDU5_DEV_NAME		"edu5"
#define EDU5_MAX_FACTORIAL	12

/* GET_INFO 返回的信息 */
struct edu5_info {
	__u32 bar_size;		/* BAR0 大小（字节） */
	__u32 irq;		/* 中断号 */
	__u32 irq_count;	/* 累计中断次数 */
	__u32 mmap_size;	/* 可 mmap 的大小（字节） */
	__u32 reserved[4];	/* 预留，保证 ABI 兼容 */
};

/* FACTORIAL 入参/出参 */
struct edu5_fact_arg {
	__u32 n;		/* 入参：0~12 */
	__u32 result;		/* 出参：n! */
	__u32 reserved[2];
};

#define EDU5_IOC_MAGIC		'E'

#define EDU5_IOC_GET_INFO	_IOR(EDU5_IOC_MAGIC, 0x01, struct edu5_info)
#define EDU5_IOC_FACTORIAL	_IOWR(EDU5_IOC_MAGIC, 0x02, struct edu5_fact_arg)
#define EDU5_IOC_DMA_TEST	_IOW(EDU5_IOC_MAGIC, 0x03, __u32)
#define EDU5_IOC_GET_MAP_SIZE	_IOR(EDU5_IOC_MAGIC, 0x04, __u32)

#define EDU5_IOC_MAXNR		0x04

#endif /* _EDU_UAPI_H */
