// SPDX-License-Identifier: GPL-2.0
/*
 * week1-hello.c —— 第 1 周：最小可加载内核模块
 *
 * 目标：跑通「写代码 -> 编译 -> insmod -> dmesg 看输出 -> rmmod」闭环。
 * 这一周不需要任何硬件，只是找回内核心跳。
 *
 * 编译：make
 * 加载：sudo insmod hello.ko
 * 查看：sudo dmesg | tail -3
 * 卸载：sudo rmmod hello
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>

static int __init hello_init(void)
{
	pr_info("loaded: jiffies=%lu\n", jiffies);
	pr_info("请在 /proc/kallsyms 里找找 hello_init 的地址：\n");
	return 0;
}

static void __exit hello_exit(void)
{
	pr_info("unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("driver-prep");
MODULE_DESCRIPTION("Week 1: minimal kernel module");
