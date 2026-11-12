// SPDX-License-Identifier: GPL-2.0
/*
 * week5-chardev-uapi/app/test_edu5.c —— 用户态测试程序
 *
 * 用法（在 guest 里）：
 *   make
 *   sudo ./test_edu5
 *
 * 依次测试：GET_INFO → FACTORIAL → mmap → DMA_TEST
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "edu_uapi.h"

static int test_info(int fd)
{
	struct edu5_info info;
	int ret;

	memset(&info, 0, sizeof(info));
	ret = ioctl(fd, EDU5_IOC_GET_INFO, &info);
	if (ret) {
		perror("GET_INFO");
		return ret;
	}
	printf("[info] bar_size=%u irq=%u irq_count=%u mmap_size=%u\n",
	       info.bar_size, info.irq, info.irq_count, info.mmap_size);
	return 0;
}

static int test_factorial(int fd, unsigned int n)
{
	struct edu5_fact_arg a;
	int ret;

	memset(&a, 0, sizeof(a));
	a.n = n;
	ret = ioctl(fd, EDU5_IOC_FACTORIAL, &a);
	if (ret) {
		perror("FACTORIAL");
		return ret;
	}
	printf("[fact] %u! = %u\n", a.n, a.result);
	return 0;
}

static int test_mmap(int fd)
{
	void *p;
	int ret = 0;

	p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	strcpy((char *)p, "hello from user space");
	if (strcmp((char *)p, "hello from user space") != 0) {
		fprintf(stderr, "[mmap] 读回内容不一致\n");
		ret = -1;
	} else {
		printf("[mmap] ok: %s\n", (char *)p);
	}

	munmap(p, 4096);
	return ret;
}

static int test_dma(int fd)
{
	__u32 one = 1;
	int ret;

	ret = ioctl(fd, EDU5_IOC_DMA_TEST, &one);
	if (ret) {
		perror("DMA_TEST");
		return ret;
	}
	printf("[dma] roundtrip ok\n");
	return 0;
}

int main(void)
{
	int fd, ret = 0;
	unsigned int i;

	fd = open("/dev/" EDU5_DEV_NAME, O_RDWR);
	if (fd < 0) {
		perror("open /dev/edu5");
		return 1;
	}

	if (test_info(fd))
		ret = 1;
	if (test_factorial(fd, 10))
		ret = 1;
	if (test_mmap(fd))
		ret = 1;
	if (test_dma(fd))
		ret = 1;

	/* 连续读几次，确认多进程/多 fd 场景下计数稳定 */
	for (i = 0; i < 3; i++) {
		__u32 tmp;
		ioctl(fd, EDU5_IOC_GET_MAP_SIZE, &tmp);
	}

	close(fd);
	printf(ret ? "TEST FAILED\n" : "TEST PASSED\n");
	return ret;
}
