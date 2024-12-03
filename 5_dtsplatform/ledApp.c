/***************************************************************
 Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
 文件名 		: ledApp.c
 作者			: 邓涛
 版本			: V1.0
 描述			: LED驱动测试源文件
 其他			: 无
 使用方法		: ./ledApp /dev/myled 0 关闭LED
			  ./ledApp /dev/myled 1 打开LED       
 论坛			: www.openedv.com
 日志			: 初版V1.0 2019/1/30 邓涛创建
 ***************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

/*
 * @description		: main主程序
 * @param - argc	: argv数组元素个数
 * @param - argv	: 具体参数
 * @return			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int fd, ret;
	unsigned char buf[1];

	if(3 != argc) {
		printf("Usage:\n"
		"\t./ledApp /dev/myled 1		@ close LED\n"
		"\t./ledApp /dev/myled 0		@ open LED\n"
		);
		return -1;
	}

	/* 打开设备 */
	fd = open(argv[1], O_RDWR);
	if(0 > fd) {
		printf("file %s open failed!\r\n", argv[1]);
		return -1;
	}

	/* 将字符串转换为int型数据 */
	buf[0] = atoi(argv[2]);

	/* 向驱动写入数据 */
	ret = write(fd, buf, sizeof(buf));
	if(0 > ret){
		printf("LED Control Failed!\r\n");
		close(fd);
		return -1;
	}

	/* 关闭设备 */
	close(fd);
	return 0;
}
