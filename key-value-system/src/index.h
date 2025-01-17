/*============================================================================
# Author: Wade Leng
# E-mail: wade.hit@gmail.com
# Last modified:	2012-02-04 16:59
# Filename:		index.h
# Description: 
============================================================================*/
#ifndef KVS_INDEX
#define KVS_INDEX

#include "type.h"

/**
 * @brief 封装index.c中值（value）的信息
 * 与buffer.h中的buffer_word对应
*/
typedef struct IDX_VALUE_INFO
{
	int		value_size; // 数据的大小（字节）
	PTR_BUF		buf_ptr; //
	OFFSET_T	disk_offset;
}IDX_VALUE_INFO;

int idx_init(const char* image, INIT_TYPE init_type);
int idx_insert(const char* key, int key_size, IDX_VALUE_INFO** value_info);
int idx_search(const char* key, int key_size, IDX_VALUE_INFO** value_info);
int idx_delete(const char* key, int key_size, IDX_VALUE_INFO* value_info);
int idx_exit();

#endif
