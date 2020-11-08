/*============================================================================
# Author: Wade Leng
# E-mail: wade.hit@gmail.com
# Last modified:	2012-02-04 16:58
# Filename:		sync.c
# Description: 
============================================================================*/
// 是为了打开超大文件，与下面的open中的O_LARGEFILE配合使用
// TODO:不知道原理是什么
#define	_FILE_OFFSET_BITS	64
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "type.h"
#include "layout.h"
#include "sync.h"
#include "log.h"

static int fd = 0;
/*
	加了extern后，编译器会在调用log_file后面查找log_file的初始化，如果查不到，就会去别的文件去找
	index.c和buffer.c都有log_file，但是它们都没有初始化的语句
	interface.c里有初始化语句，但是初始化为了NULL，interface里有init函数是真正初始化的
*/
extern FILE* log_file;

/**
 * @brief 通过文open函数，建立起一个从到文件路径到设备的访问路径
	主要是规定了访问文件的模式和权限
 * @param pathname 数据存储的文件地址
 * @return 成功返回0，失败返回-1
*/
int sync_init(char* pathname)
{
	/*
	* 以某种模式，根据输入的文件路径，生成可cha'z
	* @param file:
	* @param oflag: 代表Linux打开文件时的处理方式
		O_RDWR: 可读可写
		O_LARGEFILE: 打开超大型文件
		O_CREAT: 如果文件不存在，则创建
	* @parm mode: 规定Linux打开文件的权限，只有在第二个参数中带有O_CREAT时才生效
		计算方法: mode&~umask
		R: 读的权限，W: 写的权限，X: 执行的权限
		USR: 用户
		GRP: 组
		OTH: 其它用户
		S_IRUSR: 用户具有读的权限
		总的来说就是用户和组都要读和写的权限，没有执行的权限（数据库没有可执行文件）
	*/
	fd = open(pathname, O_RDWR | O_LARGEFILE | O_CREAT , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP); 
	if (fd <= 0) // 如果获取文件描述符失败
	{
		log_err(__FILE__, __LINE__, log_file, "sync_init---open file fail.");
		return -1;
	}

	log_err(__FILE__, __LINE__, log_file, "SYNC INIT SUCCESS.");
	return 0;
}


/**
 * @brief 在mem中，从偏移disk_offset处开始，取size大小的的文件到缓冲区mem
 * @param mem 要接收数据的缓冲区指针
 * @param size 要读取的字节数
 * @param disk_offset OFFSET_T是个使用 typedef定义的long long类型，从文件哪里开始读
 * @return 成功返回0，失败返回-1
*/
int sync_read(char* mem, int size, OFFSET_T disk_offset)
{
	// off64是一个long int，用来表示执行lseek后文件偏移
	off64_t ret;
	ssize_t n = 0, toread;
	char* ptr;
	
	// 如果文件描述符不合法
	if (fd <= 0)
	{
		log_err(__FILE__, __LINE__, log_file, "sync_read---fd <= 0.");
		return -1;
	}
	
	/**
	 * @brief lseek64和lseek的区别：
		当需要在大于2G的文件中跳转或在更大的块设备中跳转的时候lseek是无法完成任务的，这需要使用其他的文件跳转系统调用。
		LINUX中有系统调用llseek，用他可以实现64位的跳转，完全可以支持现在最大的文件或块文件的大小。而lseek64是_llseek的包装函数
	 * @param fd 文件描述符，第二个是，第三个是偏移模式 
	 * @param offset 偏移量
	 * @param whence (从)何处(开始偏移)
		模式是SEEK_SET: 文件当前位置=文件头+offset（第二个参数），相当于从头开始偏移
		模式是SEEK_CUR: 文件当前位置=文件当前位置+offset，相当于继续上次的偏移
		模式是SEEK_END: 文件当前位置=文件尾+offset，相当于从尾部开始偏移
	 * @return 成功返回文件移动后的偏移量，失败返回-1
	*/
	ret = lseek64(fd, (off64_t)disk_offset, SEEK_SET);
	if (ret < 0)
	{
		log_err(__FILE__, __LINE__, log_file, "sync_read---lseek fail.");
		return -1;
	}

	// 因为不是传入的指针，为了不破坏原有的值，特意赋值给两个临时的变量
	toread = size;
	// 这里传入的是个指针变量，所以即使换了一个名字，对新指针变量的操作还是会影响到原来的
	ptr = mem;
	// 如果还没取完
	while (toread > 0) 
	{
		/**
		 * @brief 由以打开的文件读取数据，会改变文件偏移位置
		 * @param fd 文件描述符
		 * @param buf 读取后要存储的缓冲区地址
		 * @param count 读取到buf的字节数，如果值为0，则此函数将不进行读取
		 * @return: 实际读取的字节数，为0说明已经到末尾或者无可读取的数据
		*/
		n = read(fd, ptr, size); // 从文件中读取size个字节的数据到ptr缓冲区
		toread -= n; // 减少读取完的字节
		ptr += n;  //应该是缓冲区指针偏移相应字节的位数，以迎接下一次的读取
	}

	return 0;
}

/**
 * @brief 在mem中，从偏移disk_offset处开始，写size大小的缓冲区mem到文件
 * @param mem 要写入数据的所存储的缓冲区指针
 * @param size 要写入的字节数
 * @param disk_offset OFFSET_T是个使用 typedef定义的long long类型，从文件哪里开始写
 * @return 成功返回0，失败返回-1
*/
int sync_write(const char* mem, int size, OFFSET_T disk_offset)
{
	off64_t ret;
	ssize_t n, towrite;
	const char* ptr;

	if (fd <= 0) 
	{
		log_err(__FILE__, __LINE__, log_file, "sync_write---fd <= 0.");
		return -1;
	}

	ret = lseek64(fd, (off64_t)disk_offset, SEEK_SET);
	if (ret < 0) 
	{
		log_err(__FILE__, __LINE__, log_file, "sync_write---lseek fail. %d", disk_offset);
		return -1;
	}

	towrite = size;
	ptr = mem;
	while (towrite > 0)
	{
		n = write(fd, ptr, size); 
		towrite -= n;
		ptr += n;
	}

	return 0;
}

/**
 * @brief 类似一个测试，完成了
	1.把文件路径转化为文件描述符（将文件加载到Linux系统中）
	2.从文件开头，重置文件偏移量
	3.读取size字节的数据到缓冲区g_image，测试可读性
 * @param file_name 文件路径
 * @param g_image 接收文件读取的缓冲区指针
 * @param size 读取的字节数
 * @return 成功返回0，失败返回-1
*/
int sync_image_load(const char* file_name, char* g_image, int size)
{
	off_t ret;
	ssize_t n, toread;
	char* ptr;
	int image_fd;

	// 以可读可写的方式打开文件，如果文件不存在，则新建文件用户和组可读可写
	//? 这里第二个参数不带O_CREAT，按道理第三个参数应该无效地，为什么还要写呢？
	image_fd = open(file_name, O_RDWR , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP); 
	if (image_fd <= 0) 
	{
		log_err(__FILE__, __LINE__, log_file, "sync_image_load---fd <= 0.");
		return -1;
	}

	// 重置文件偏移量
	ret = lseek(image_fd, 0, SEEK_SET);
	if (ret < 0)
	{
		log_err(__FILE__, __LINE__, log_file, "sync_image_load---lseek fail.");
		return -1;
	}
	// 和sync_read一样，作用是把缓冲区的数据写入到文件中
	toread = size;
	ptr = g_image;
	while (toread > 0)
	{
		n = read(image_fd, ptr, size);
		toread -= n;
		ptr += n;
	}

	return 0;
}


/**
 * @brief 和sync_write很像，作用是从缓冲区g_image写入size字节数据到文件file_name，和sync_write的区别有：
 *   1.文件描述符来自函数的形参，而不是此文件中的全局变量
 *   2.不可以通过形参设置偏移量，也就是不能自己定义从哪里开始读，统一从头开始读
 * 作者在文档中提到它还在此文件中设置了供index调用的持久化接口，有可能与此有关
 * @param file_name 要写入的文件路径指针
 * @param g_image 要写入数据所在的缓冲区指针
 * @param size 要写入的文件字节
 * @return 成功返回0，失败返回-1
*/
int sync_image_save(const char* file_name, char* g_image, int size)
{
	off_t ret;
	ssize_t n, towrite;
	char* ptr;
	int image_fd;
	
	// 使用可读可写方式打开，不存在则创建，创建的文件，用户和组可读可写
	image_fd = open(file_name, O_RDWR | O_CREAT , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP); 
	if (image_fd <= 0) 
	{
		log_err(__FILE__, __LINE__, log_file, "sync_image_save---fd <= 0.");
		return -1;
	}

	// 重置文件偏移量到文件开头
	ret = lseek(image_fd, 0, SEEK_SET);
	if (ret < 0) 
	{
		log_err(__FILE__, __LINE__, log_file, "sync_image_save---lseek fail.");
		return -1;
	}

	// 从缓冲区ptr写入size字节的数据到文件
	towrite = size;
	ptr = g_image;
	while (towrite > 0)
	{
		n = write(image_fd, ptr, size); 
		towrite -= n;
		ptr += n;
	}

	return 0;
}

/**
 * @brief 退出，本质上是关闭文件描述符
 * @return 成功返回0，失败返回-1
*/
int sync_exit()
{
	if (log_file)
		log_err(__FILE__, __LINE__, log_file, "SYNC EXIT.");
	// 关闭文件描述符，关闭后此文件就不能被操作，除非使用open函数重新打开
	close(fd);

	return 0;
}