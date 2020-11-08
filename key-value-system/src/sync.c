/*============================================================================
# Author: Wade Leng
# E-mail: wade.hit@gmail.com
# Last modified:	2012-02-04 16:58
# Filename:		sync.c
# Description: 
============================================================================*/
// ��Ϊ�˴򿪳����ļ����������open�е�O_LARGEFILE���ʹ��
// TODO:��֪��ԭ����ʲô
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
	����extern�󣬱��������ڵ���log_file�������log_file�ĳ�ʼ��������鲻�����ͻ�ȥ����ļ�ȥ��
	index.c��buffer.c����log_file���������Ƕ�û�г�ʼ�������
	interface.c���г�ʼ����䣬���ǳ�ʼ��Ϊ��NULL��interface����init������������ʼ����
*/
extern FILE* log_file;

/**
 * @brief ͨ����open������������һ���ӵ��ļ�·�����豸�ķ���·��
	��Ҫ�ǹ涨�˷����ļ���ģʽ��Ȩ��
 * @param pathname ���ݴ洢���ļ���ַ
 * @return �ɹ�����0��ʧ�ܷ���-1
*/
int sync_init(char* pathname)
{
	/*
	* ��ĳ��ģʽ������������ļ�·�������ɿ�cha'z
	* @param file:
	* @param oflag: ����Linux���ļ�ʱ�Ĵ���ʽ
		O_RDWR: �ɶ���д
		O_LARGEFILE: �򿪳������ļ�
		O_CREAT: ����ļ������ڣ��򴴽�
	* @parm mode: �涨Linux���ļ���Ȩ�ޣ�ֻ���ڵڶ��������д���O_CREATʱ����Ч
		���㷽��: mode&~umask
		R: ����Ȩ�ޣ�W: д��Ȩ�ޣ�X: ִ�е�Ȩ��
		USR: �û�
		GRP: ��
		OTH: �����û�
		S_IRUSR: �û����ж���Ȩ��
		�ܵ���˵�����û����鶼Ҫ����д��Ȩ�ޣ�û��ִ�е�Ȩ�ޣ����ݿ�û�п�ִ���ļ���
	*/
	fd = open(pathname, O_RDWR | O_LARGEFILE | O_CREAT , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP); 
	if (fd <= 0) // �����ȡ�ļ�������ʧ��
	{
		log_err(__FILE__, __LINE__, log_file, "sync_init---open file fail.");
		return -1;
	}

	log_err(__FILE__, __LINE__, log_file, "SYNC INIT SUCCESS.");
	return 0;
}


/**
 * @brief ��mem�У���ƫ��disk_offset����ʼ��ȡsize��С�ĵ��ļ���������mem
 * @param mem Ҫ�������ݵĻ�����ָ��
 * @param size Ҫ��ȡ���ֽ���
 * @param disk_offset OFFSET_T�Ǹ�ʹ�� typedef�����long long���ͣ����ļ����￪ʼ��
 * @return �ɹ�����0��ʧ�ܷ���-1
*/
int sync_read(char* mem, int size, OFFSET_T disk_offset)
{
	// off64��һ��long int��������ʾִ��lseek���ļ�ƫ��
	off64_t ret;
	ssize_t n = 0, toread;
	char* ptr;
	
	// ����ļ����������Ϸ�
	if (fd <= 0)
	{
		log_err(__FILE__, __LINE__, log_file, "sync_read---fd <= 0.");
		return -1;
	}
	
	/**
	 * @brief lseek64��lseek������
		����Ҫ�ڴ���2G���ļ�����ת���ڸ���Ŀ��豸����ת��ʱ��lseek���޷��������ģ�����Ҫʹ���������ļ���תϵͳ���á�
		LINUX����ϵͳ����llseek����������ʵ��64λ����ת����ȫ����֧�����������ļ�����ļ��Ĵ�С����lseek64��_llseek�İ�װ����
	 * @param fd �ļ����������ڶ����ǣ���������ƫ��ģʽ 
	 * @param offset ƫ����
	 * @param whence (��)�δ�(��ʼƫ��)
		ģʽ��SEEK_SET: �ļ���ǰλ��=�ļ�ͷ+offset���ڶ������������൱�ڴ�ͷ��ʼƫ��
		ģʽ��SEEK_CUR: �ļ���ǰλ��=�ļ���ǰλ��+offset���൱�ڼ����ϴε�ƫ��
		ģʽ��SEEK_END: �ļ���ǰλ��=�ļ�β+offset���൱�ڴ�β����ʼƫ��
	 * @return �ɹ������ļ��ƶ����ƫ������ʧ�ܷ���-1
	*/
	ret = lseek64(fd, (off64_t)disk_offset, SEEK_SET);
	if (ret < 0)
	{
		log_err(__FILE__, __LINE__, log_file, "sync_read---lseek fail.");
		return -1;
	}

	// ��Ϊ���Ǵ����ָ�룬Ϊ�˲��ƻ�ԭ�е�ֵ�����⸳ֵ��������ʱ�ı���
	toread = size;
	// ���ﴫ����Ǹ�ָ����������Լ�ʹ����һ�����֣�����ָ������Ĳ������ǻ�Ӱ�쵽ԭ����
	ptr = mem;
	// �����ûȡ��
	while (toread > 0) 
	{
		/**
		 * @brief ���Դ򿪵��ļ���ȡ���ݣ���ı��ļ�ƫ��λ��
		 * @param fd �ļ�������
		 * @param buf ��ȡ��Ҫ�洢�Ļ�������ַ
		 * @param count ��ȡ��buf���ֽ��������ֵΪ0����˺����������ж�ȡ
		 * @return: ʵ�ʶ�ȡ���ֽ�����Ϊ0˵���Ѿ���ĩβ�����޿ɶ�ȡ������
		*/
		n = read(fd, ptr, size); // ���ļ��ж�ȡsize���ֽڵ����ݵ�ptr������
		toread -= n; // ���ٶ�ȡ����ֽ�
		ptr += n;  //Ӧ���ǻ�����ָ��ƫ����Ӧ�ֽڵ�λ������ӭ����һ�εĶ�ȡ
	}

	return 0;
}

/**
 * @brief ��mem�У���ƫ��disk_offset����ʼ��дsize��С�Ļ�����mem���ļ�
 * @param mem Ҫд�����ݵ����洢�Ļ�����ָ��
 * @param size Ҫд����ֽ���
 * @param disk_offset OFFSET_T�Ǹ�ʹ�� typedef�����long long���ͣ����ļ����￪ʼд
 * @return �ɹ�����0��ʧ�ܷ���-1
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
 * @brief ����һ�����ԣ������
	1.���ļ�·��ת��Ϊ�ļ������������ļ����ص�Linuxϵͳ�У�
	2.���ļ���ͷ�������ļ�ƫ����
	3.��ȡsize�ֽڵ����ݵ�������g_image�����Կɶ���
 * @param file_name �ļ�·��
 * @param g_image �����ļ���ȡ�Ļ�����ָ��
 * @param size ��ȡ���ֽ���
 * @return �ɹ�����0��ʧ�ܷ���-1
*/
int sync_image_load(const char* file_name, char* g_image, int size)
{
	off_t ret;
	ssize_t n, toread;
	char* ptr;
	int image_fd;

	// �Կɶ���д�ķ�ʽ���ļ�������ļ������ڣ����½��ļ��û�����ɶ���д
	//? ����ڶ�����������O_CREAT�����������������Ӧ����Ч�أ�Ϊʲô��Ҫд�أ�
	image_fd = open(file_name, O_RDWR , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP); 
	if (image_fd <= 0) 
	{
		log_err(__FILE__, __LINE__, log_file, "sync_image_load---fd <= 0.");
		return -1;
	}

	// �����ļ�ƫ����
	ret = lseek(image_fd, 0, SEEK_SET);
	if (ret < 0)
	{
		log_err(__FILE__, __LINE__, log_file, "sync_image_load---lseek fail.");
		return -1;
	}
	// ��sync_readһ���������ǰѻ�����������д�뵽�ļ���
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
 * @brief ��sync_write���������Ǵӻ�����g_imageд��size�ֽ����ݵ��ļ�file_name����sync_write�������У�
 *   1.�ļ����������Ժ������βΣ������Ǵ��ļ��е�ȫ�ֱ���
 *   2.������ͨ���β�����ƫ������Ҳ���ǲ����Լ���������￪ʼ����ͳһ��ͷ��ʼ��
 * �������ĵ����ᵽ�����ڴ��ļ��������˹�index���õĳ־û��ӿڣ��п�������й�
 * @param file_name Ҫд����ļ�·��ָ��
 * @param g_image Ҫд���������ڵĻ�����ָ��
 * @param size Ҫд����ļ��ֽ�
 * @return �ɹ�����0��ʧ�ܷ���-1
*/
int sync_image_save(const char* file_name, char* g_image, int size)
{
	off_t ret;
	ssize_t n, towrite;
	char* ptr;
	int image_fd;
	
	// ʹ�ÿɶ���д��ʽ�򿪣��������򴴽����������ļ����û�����ɶ���д
	image_fd = open(file_name, O_RDWR | O_CREAT , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP); 
	if (image_fd <= 0) 
	{
		log_err(__FILE__, __LINE__, log_file, "sync_image_save---fd <= 0.");
		return -1;
	}

	// �����ļ�ƫ�������ļ���ͷ
	ret = lseek(image_fd, 0, SEEK_SET);
	if (ret < 0) 
	{
		log_err(__FILE__, __LINE__, log_file, "sync_image_save---lseek fail.");
		return -1;
	}

	// �ӻ�����ptrд��size�ֽڵ����ݵ��ļ�
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
 * @brief �˳����������ǹر��ļ�������
 * @return �ɹ�����0��ʧ�ܷ���-1
*/
int sync_exit()
{
	if (log_file)
		log_err(__FILE__, __LINE__, log_file, "SYNC EXIT.");
	// �ر��ļ����������رպ���ļ��Ͳ��ܱ�����������ʹ��open�������´�
	close(fd);

	return 0;
}