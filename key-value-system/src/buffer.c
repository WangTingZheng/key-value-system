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
 * buf_pool buffer �ص��׵�ַ����buffer��д����Ҫ��ͷ����д��ʱ��last_ptr���õ���λ��
 * last_ptr buffer ���п��пռ���׵�ַ��������д��ֵʱ��������ʼд��д��һ���ƶ���ֵʱ�ƶ���ֵĩβ
 * 
 * exploit_ptr
 * waste_ptr
 * last_waste_ptr
 * last_flush_ptr
*/
static	PTR_BUF		buf_pool = NULL, exploit_ptr, waste_ptr, last_waste_ptr, last_ptr, last_flush_ptr;

/**
 * @brief 
 * first_flag ��ʶ�Ƿ��һ�Σ�����ǵ�һ����buffer������Ļ������пռ䶼����Ҫ���أ�ֱ��װ��ȥ
 * rest_space ������ǵ�һ�δ���Ļ����͵ô�ͷ��ʼ������value�ĳߴ粻һ������Ȼ���ƻ���һ�η���õĽṹ��
 * ���ƻ������ݵ�Ԫǰ�汻�˴ε����ݸ��ǣ��󲿷ֿ������޷�ʹ�ã������ʣ�µĿռ�
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
	// �趨Ϊ���ڵ�һ��
	first_flag = 1;	
	// 
	not_flush_size = rest_space = 0;
	exit_flag = 0;
	// �趨�������Ϊ0
	diff = 0;

	// ����������βγ�ʼ��buffer����С��˯��ʱ�䡢buffer���ڴ�С�Ȳ���
	buffer_total_size = buf_size;
	sleep_time = buffer_sleep_time;
	buffer_horizon_size = horizon_size;

	// �趨����ָ���ַ�����һ��ˢ��ָ���ַ����value��������ָ���ַ��buffer����ͷ��ַΪbuffer�����׵�ַ
	exploit_ptr = last_flush_ptr = last_ptr = buf_pool = (PTR_BUF)buffer_mem;
	// �����һ�λķ������׵�ַ���ķ����׵�ַ��ʼ��Ϊbuffer���׵�ַ����buffer����С
	last_waste_ptr = waste_ptr = (PTR_BUF)(buf_pool + buffer_total_size);
	//? ���ܺ�buffer������ַ�й�
	disk_offset = DISK_VALUE_OFFSET;		

	/**
	 * @brief ����һ���߳�
	 * @param tidp �洢�߳�id ���ڴ�ָ�룬�̴߳����ɹ�����߳�id�洢�ڸò�����ָʾ�ĵ�ַ��
	 * @param attr �߳����ԣ�����NULL��ʹ��Ĭ������ 
	 * @param start_rtn �߳�����ʱִ�еĺ�����ָ�� 
	 * @param arg �߳�����ʱ��start_rtn�������ݵĲ��� 
	 * @return �ɹ�����0��ʧ�ܷ��ش�����
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
	*  �����put֮��last_ptr�ĵ�ַ
	*  �ұ���bufferĩβ�ĵ�ַ
	*  ���put֮��ĵ�ַ����bufferĩβ�ĵ�ַ��˵��put������
	*/
	if (last_ptr + value_size + word_size > (PTR_BUF)(buf_pool + buffer_total_size - 1))
	{
		// ����ǵ�һ��put��
		if (first_flag)
			last_waste_ptr = waste_ptr = last_ptr;
		// ������ǵ�һ��put
		else
		{
			// ����ǵڶ��֣������˵�diff = 1�֣�
			if (diff == 1)
			{
				// ����last_waste_ptr
				last_waste_ptr = waste_ptr;
				// ���һ��value֮���ǻķ��� 
				// ���һ��value��ĩβָ����ǻķ������׵�ַ
				waste_ptr = last_ptr;
			}
			// ����ǵ�һ�֣���ifһ��������ʵ�����Ǹ�bug
			else if (diff == 0)
				last_waste_ptr = waste_ptr = last_ptr;
		}
		// ��ʼ׼����һ��
		// ����ʣ��ռ����³�ʼ��
		rest_space = 0;
		// ������ָ���βָ�����»ص�buffer���ײ�
		last_ptr = exploit_ptr = buf_pool;
		// �����Ƿ񾭹���һ�ֱ�־Ϊ��
		first_flag = 0;
		// ����ظ�������һ
		pthread_mutex_lock(&mutex);
		diff++;
		pthread_mutex_unlock(&mutex);
	}
	// ����ǵ�һ�֣���һ��if elseֻ�����˵�һ��
	if (first_flag)		//first lap
	{
		// buffer����put֮�������ݵ�
		buf_word_ptr = (buf_word*)last_ptr;
		// ��һ�ο��ص�ĩβ����Ҫ����word��Ϣ��Ĵ�С��Ӧ�þ���value����׵�ַ
		*buf_value_ptr = (PTR_BUF)(last_ptr + word_size);
		// word��Ϣ��Ĵ�С����value��Ĵ�С�������ʸ��Ĵ�С��ԭ���Ŀ��ص�ĩβ������������put���µ�ĩβ
		last_ptr += (word_size + value_size);
		// �������µĿ��ص�ַ
		exploit_ptr = last_ptr;

		// ���µ�word�鸳ֵ��ֵ���Ժ������β�
		buf_word_ptr->value_info_ptr = value_info_ptr;
		// Ĭ����δˢ�µ�
		buf_word_ptr->state = p_not_flush;					//not flush to disk
		buf_word_ptr->value_size = value_size;
		// ����ֵ��ֵ�Ĵ�С��value��
		memcpy(*buf_value_ptr, value, value_size);
		// ��value�鸳ֵ
		(*value_info_ptr).value_size = value_size;
		(*value_info_ptr).buf_ptr = *buf_value_ptr;
		(*value_info_ptr).disk_offset = DISK_OFFSET_NULL;
		// ��Ϊ�¼�����һ��δˢ�µ����ݣ����������δˢ��������Ϊ���߳�����ʵʱ����
		pthread_mutex_lock(&mutex);
		not_flush_size += value_size;
		pthread_mutex_unlock(&mutex);
		return 0;
	}
	// ����Ѿ����پ���һ��
	else
	{
		// ʹ��һ����ʱ������סʣ���С
		avail_space = rest_space;						//last remain space
		// ���ʣ��Ĵ�СС��������Ĵ�С��˵��û�취���������
		while (avail_space < value_size + word_size)				//exploit space
		{
			// ������ص��˻ķ���
			if (exploit_ptr >= waste_ptr)					//only use for just last once
			{
				// ��������ĩβָ�븳ֵ��word���׵�ַ
				// ��������ĩβָ�븳ֵ��word���׵�ַ
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
 * @brief �߳�����ʱִ�еĺ���
 * @return 
*/
static void* buffer_lookout()
{
	// ������ˢ��������Ҫˢ��������ֵ�ߴ�
	int flushed, to_flush, value_size;
	// ��������ͷ
	buf_word* buf_word_ptr = NULL;
	// ����
	PTR_BUF buf_value_ptr;
	int state;

	// ���˳�flag����0�����˳������߻�������δˢ��ʱ
	while(!exit_flag || not_flush_size > 0)
	{
		// ���δˢ�����ݴ�С���趨����ֵ�������Ҫ�˳�ʱ��˵��
		if (not_flush_size >= buffer_horizon_size || exit_flag)  //XXX: || diff == 2
		{
			// δˢ�£�û���Ѿ�ˢ����ɵģ����Գ�ʼ����ˢ������Ϊ0
			flushed = 0;
			// ������趨Ҫˢ�µ���Ϊδˢ�µ���
			// 
			pthread_mutex_lock(&mutex);
			to_flush = not_flush_size;
			not_flush_size = 0;
			pthread_mutex_unlock(&mutex);
			// ����ˢ�µ�С��Ҫˢ�µ���������ˢ�µ�δ��������һ�룩
			while(flushed < to_flush)
			{
				// ����ϴ�ˢ��value�ĵ�ַ���ڻķ����ײ�
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