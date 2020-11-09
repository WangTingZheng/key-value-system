/*============================================================================
# Author: Wade Leng
# E-mail: wade.hit@gmail.com
# Last modified:	2012-02-04 16:58
# Filename:		buffer.c
# Description: 
============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "layout.h"
#include "type.h"
#include "index.h"
#include "buffer.h"
#include "sync.h"
#include "log.h"

OFFSET_T		disk_offset;
const	static	int	word_size = sizeof(buf_word);
extern	FILE*		log_file; 
/**
 * @brief 
 * buf_pool buffer 池的首地址，当buffer池写满需要从头覆盖写的时候，last_ptr重置到此位置
 * last_ptr buffer 池中空闲空间的首地址，当继续写入值时，从它开始写，写完一次移动到值时移动到值末尾
 * 
 * exploit_ptr
 * waste_ptr
 * last_waste_ptr
 * last_flush_ptr
*/
static	PTR_BUF		buf_pool = NULL, exploit_ptr, waste_ptr, last_waste_ptr, last_ptr, last_flush_ptr;

/**
 * @brief 
 * first_flag 标识是否第一次，如果是第一次在buffer区存入的话，所有空间都不需要开拓，直接装进去
 * rest_space 如果不是第一次存入的话，就得从头开始，由于value的尺寸不一样，必然会破坏上一次分配好的结构，
 * 被破坏的数据单元前面被此次的数据覆盖，后部分空余且无法使用，这就是剩下的空间
*/
static	int		first_flag, rest_space = 0;
static	int		sleep_time, not_flush_size, buffer_horizon_size;
static	int		buffer_total_size, exit_flag;
static	pthread_t	tid;
static	pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;
enum	diff_t 		{same, one, two};
enum	diff_t		diff;


static	void*		buffer_lookout();

/**
 * @brief 
 * @param buffer_mem 
 * @param buf_size 
 * @param buffer_sleep_time 
 * @param horizon_size 
 * @return 
*/
int buffer_init(const char* buffer_mem, const int buf_size, const int buffer_sleep_time, const int horizon_size)
{
	if (horizon_size >= buf_size)		//for safe
	{
		log_err(__FILE__, __LINE__, log_file, "horizon_size is too large, DANGEROUS.");
		return -1;
	}
	// 设定为还在第一轮
	first_flag = 1;	
	// 
	not_flush_size = rest_space = 0;
	exit_flag = 0;
	// 设定完成轮数为0
	diff = 0;

	// 根据输入的形参初始化buffer区大小、睡眠时间、buffer窗口大小等参数
	buffer_total_size = buf_size;
	sleep_time = buffer_sleep_time;
	buffer_horizon_size = horizon_size;

	// 设定开拓指针地址、最后一次刷新指针地址、有value的最后面的指针地址、buffer区开头地址为buffer数组首地址
	exploit_ptr = last_flush_ptr = last_ptr = buf_pool = (PTR_BUF)buffer_mem;
	// 将最后一次荒废区的首地址、荒废区首地址初始化为buffer区首地址加上buffer区大小
	last_waste_ptr = waste_ptr = (PTR_BUF)(buf_pool + buffer_total_size);
	//? 可能和buffer的外存地址有关
	disk_offset = DISK_VALUE_OFFSET;		

	/**
	 * @brief 创建一个线程
	 * @param tidp 存储线程id 的内存指针，线程创建成功会把线程id存储在该参数所指示的地址中
	 * @param attr 线程属性，传入NULL来使用默认属性 
	 * @param start_rtn 线程启动时执行的函数的指针 
	 * @param arg 线程启动时往start_rtn函数传递的参数 
	 * @return 成功返回0，失败返回错误码
	*/
	if (pthread_create(&tid, NULL, buffer_lookout, NULL) != 0)
	{
		log_err(__FILE__, __LINE__, log_file, "buffer_init---pthread_create fail.");
		return -1;
	}
	log_err(__FILE__, __LINE__, log_file, "BUFFER INIT SUCCESS.");

	return 0;
}

int buffer_put(const char* value, int value_size, PTR_BUF* buf_value_ptr, IDX_VALUE_INFO* value_info_ptr)
{
	buf_word* buf_word_ptr = NULL;
	buf_word* temp;
	int avail_space = 0;

	/*
	*  左边是put之后last_ptr的地址
	*  右边是buffer末尾的地址
	*  如果put之后的地址超过buffer末尾的地址，说明put不下了
	*/
	if (last_ptr + value_size + word_size > (PTR_BUF)(buf_pool + buffer_total_size - 1))
	{
		// 如果是第一轮put，
		if (first_flag)
			last_waste_ptr = waste_ptr = last_ptr;
		// 如果不是第一轮put
		else
		{
			// 如果是第二轮（结束了第diff = 1轮）
			if (diff == 1)
			{
				// 更新last_waste_ptr
				last_waste_ptr = waste_ptr;
				// 最后一个value之后都是荒废区 
				// 最后一个value的末尾指针就是荒废区的首地址
				waste_ptr = last_ptr;
			}
			// 如果是第一轮，和if一样，这里实际上是个bug
			else if (diff == 0)
				last_waste_ptr = waste_ptr = last_ptr;
		}
		// 开始准备下一趟
		// 开拓剩余空间重新初始化
		rest_space = 0;
		// 开拓首指针和尾指针重新回到buffer区首部
		last_ptr = exploit_ptr = buf_pool;
		// 设置是否经过第一轮标志为否
		first_flag = 0;
		// 互斥地给轮数加一
		pthread_mutex_lock(&mutex);
		diff++;
		pthread_mutex_unlock(&mutex);
	}
	// 如果是第一轮，上一个if else只进入了第一个
	if (first_flag)		//first lap
	{
		// buffer区中put之后有数据的
		buf_word_ptr = (buf_word*)last_ptr;
		// 上一次开拓的末尾加上要加入word信息块的大小，应该就是value块的首地址
		*buf_value_ptr = (PTR_BUF)(last_ptr + word_size);
		// word信息块的大小加上value块的大小，就是资格块的大小，原来的开拓的末尾加上它，就是put完新的末尾
		last_ptr += (word_size + value_size);
		// 它就是新的开拓地址
		exploit_ptr = last_ptr;

		// 给新的word块赋值，值来自函数的形参
		buf_word_ptr->value_info_ptr = value_info_ptr;
		// 默认是未刷新的
		buf_word_ptr->state = p_not_flush;					//not flush to disk
		buf_word_ptr->value_size = value_size;
		// 拷贝值和值的大小到value块
		memcpy(*buf_value_ptr, value, value_size);
		// 给value块赋值
		(*value_info_ptr).value_size = value_size;
		(*value_info_ptr).buf_ptr = *buf_value_ptr;
		(*value_info_ptr).disk_offset = DISK_OFFSET_NULL;
		// 因为新加入了一个未刷新地数据，互斥地增加未刷新量，因为有线程正在实时监听
		pthread_mutex_lock(&mutex);
		not_flush_size += value_size;
		pthread_mutex_unlock(&mutex);
		return 0;
	}
	// 如果已经至少经过一轮
	else
	{
		// 使用一个临时变量接住剩余大小
		avail_space = rest_space;						//last remain space
		// 如果剩余的大小小于整个块的大小，说明没办法再往里放了
		while (avail_space < value_size + word_size)				//exploit space
		{
			// 如果开拓到了荒废区
			if (exploit_ptr >= waste_ptr)					//only use for just last once
			{
				// 开拓区的末尾指针赋值给word的首地址
				// 开拓区的末尾指针赋值给word的首地址
				buf_word_ptr = (buf_word*)last_ptr;
				//
				*buf_value_ptr = (PTR_BUF)(last_ptr + word_size); 
				if (diff == 1)
					waste_ptr = last_ptr + word_size + value_size;
				else if (diff == 0)
					last_waste_ptr = waste_ptr = last_ptr + word_size + value_size;
				last_ptr = exploit_ptr = buf_pool;			//restart from head
				rest_space = 0;

				buf_word_ptr->value_info_ptr = value_info_ptr;
				buf_word_ptr->state = p_not_flush;			//not flush to disk
				buf_word_ptr->value_size = value_size;
				memcpy(*buf_value_ptr, value, value_size);
				(*value_info_ptr).value_size = value_size;
				(*value_info_ptr).buf_ptr = *buf_value_ptr;
				(*value_info_ptr).disk_offset = DISK_OFFSET_NULL;
				pthread_mutex_lock(&mutex);
				not_flush_size += value_size;
				diff++;
				pthread_mutex_unlock(&mutex);
				return 0;
			}
			temp = (buf_word*)exploit_ptr;
			while(temp->state == p_not_flush || diff >= 2)
			{
				sleep(1);
			}
			if (temp->state == p_flushed)					//update value_info
				(temp->value_info_ptr)->buf_ptr = BUF_PTR_NULL;	
			avail_space += word_size + (temp->value_info_ptr)->value_size;
			exploit_ptr += word_size + (temp->value_info_ptr)->value_size; 
		}
		buf_word_ptr = (buf_word*)last_ptr;
		*buf_value_ptr = (PTR_BUF)(last_ptr + word_size);
		last_ptr = last_ptr + word_size + value_size;
		rest_space = exploit_ptr - last_ptr;
		
		buf_word_ptr->value_info_ptr = value_info_ptr;
		buf_word_ptr->state = p_not_flush;					//not flush to disk
		buf_word_ptr->value_size = value_size;
		memcpy(*buf_value_ptr, value, value_size);
		(*value_info_ptr).value_size = value_size;
		(*value_info_ptr).buf_ptr = *buf_value_ptr;
		(*value_info_ptr).disk_offset = DISK_OFFSET_NULL;
		pthread_mutex_lock(&mutex);
		not_flush_size += value_size;					
		pthread_mutex_unlock(&mutex);
		return 0;
	}
}

int buffer_get(PTR_BUF buf, int buf_size, PTR_BUF buf_value_ptr)
{
	buf_word* buf_word_ptr;
	int buf_value_size;

	buf_word_ptr = (buf_word*)(buf_value_ptr - word_size);
	buf_value_size = buf_word_ptr->value_info_ptr->value_size;

	if (buf_size < buf_value_size) 
	{
		log_err(__FILE__, __LINE__, log_file, "buffer_get---buf_size < buffer_value_size.");
		return -1;
	}
	memcpy(buf, buf_value_ptr, buf_value_size);
	buf[buf_value_size] = '\0';

	return 0;
}

int buffer_delete(PTR_BUF buf_value_ptr)
{
	buf_word* buf_word_ptr;

	buf_word_ptr = (buf_word*)(buf_value_ptr - word_size);
	buf_word_ptr->state = p_unavail;
	//XXX: not delete value @disk

	return 0;
}

int buffer_exit()
{
	if (log_file)
		log_err(__FILE__, __LINE__, log_file, "BUFFER EXIT.");

	exit_flag = 1;
	if (pthread_join(tid, NULL) != 0)
	{
		log_err(__FILE__, __LINE__, log_file, "buffer_exit---pthread_join fail.");
		return -1;
	}

	return 0;
}

/**
 * @brief 线程启动时执行的函数
 * @return 
*/
static void* buffer_lookout()
{
	// 定义已刷新数量、要刷新数量、值尺寸
	int flushed, to_flush, value_size;
	// 定义数据头
	buf_word* buf_word_ptr = NULL;
	// 定义
	PTR_BUF buf_value_ptr;
	int state;

	// 当退出flag等于0（不退出）或者还有数据未刷新时
	while(!exit_flag || not_flush_size > 0)
	{
		// 如果未刷新数据大小比设定的阈值大或者需要退出时候，说明
		if (not_flush_size >= buffer_horizon_size || exit_flag)  //XXX: || diff == 2
		{
			// 未刷新，没有已经刷新完成的，所以初始化已刷新数量为0
			flushed = 0;
			// 互斥地设定要刷新的量为未刷新的量
			// 
			pthread_mutex_lock(&mutex);
			to_flush = not_flush_size;
			not_flush_size = 0;
			pthread_mutex_unlock(&mutex);
			// 当已刷新的小于要刷新的数量（已刷新的未到总数的一半）
			while(flushed < to_flush)
			{
				// 如果上次刷新value的地址大于荒废区首部
				// 
				if (last_flush_ptr >= last_waste_ptr)
				{
					pthread_mutex_lock(&mutex);
					diff--;
					pthread_mutex_unlock(&mutex);
					// 
					last_flush_ptr = buf_pool;
					last_waste_ptr = waste_ptr;
				}
				buf_word_ptr = (buf_word*)last_flush_ptr;
				buf_value_ptr = (PTR_BUF)(last_flush_ptr + word_size);
				value_size = buf_word_ptr->value_size;
				last_flush_ptr = last_flush_ptr + word_size + value_size;
				if (buf_word_ptr->state == p_not_flush)
				{
					state = sync_write(buf_value_ptr, value_size, disk_offset);
					buf_word_ptr->value_info_ptr->disk_offset = disk_offset;
					buf_word_ptr->state = p_flushed;
					disk_offset += value_size;
				}
				flushed += value_size;
			}
		}
		else
			sleep(sleep_time);
	}
	pthread_exit(NULL);
}