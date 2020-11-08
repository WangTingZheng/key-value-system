/*============================================================================
# Author: Wade Leng
# E-mail: wade.hit@gmail.com
# Last modified:	2012-02-04 16:58
# Filename:		buffer.h
# Description: 
============================================================================*/
#ifndef KVS_BUFFER
#define KVS_BUFFER

#include "index.h"
#include "type.h"

/**
 * @brief 值（数据）在buffer里存储的格式是buf_word+value
 * buf_word结构体更多的是一些描述数据的信息
*/
typedef struct buf_word
{
	// index.h中封装的value的信息结构体
	IDX_VALUE_INFO* value_info_ptr;
	// 数据的大小（字节）
	int value_size; 
	/*
		数据的状态，有三种：
			1. unavail: 不可用
			2. flushed; 阻塞
			3. not_flush: 没有阻塞
	*/
	enum state_t state;
}buf_word;

int buffer_init(const char* buffer_mem, const int buffer_size, const int buffer_sleep_time, const int horizon_size);
int buffer_put(const char* value, int value_size, PTR_BUF* buf_value_ptr, IDX_VALUE_INFO* value_info_ptr);
int buffer_get(PTR_BUF buf, int buf_size, PTR_BUF buf_value_ptr);
int buffer_delete(PTR_BUF buf_value_ptr);
int buffer_exit();

#endif

