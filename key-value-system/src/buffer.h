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
 * @brief ֵ�����ݣ���buffer��洢�ĸ�ʽ��buf_word+value
 * buf_word�ṹ��������һЩ�������ݵ���Ϣ
*/
typedef struct buf_word
{
	// index.h�з�װ��value����Ϣ�ṹ��
	IDX_VALUE_INFO* value_info_ptr;
	// ���ݵĴ�С���ֽڣ�
	int value_size; 
	/*
		���ݵ�״̬�������֣�
			1. unavail: ������
			2. flushed; ����
			3. not_flush: û������
	*/
	enum state_t state;
}buf_word;

int buffer_init(const char* buffer_mem, const int buffer_size, const int buffer_sleep_time, const int horizon_size);
int buffer_put(const char* value, int value_size, PTR_BUF* buf_value_ptr, IDX_VALUE_INFO* value_info_ptr);
int buffer_get(PTR_BUF buf, int buf_size, PTR_BUF buf_value_ptr);
int buffer_delete(PTR_BUF buf_value_ptr);
int buffer_exit();

#endif

