# key-value-system 使用文档

## 介绍

- 项目名称：单机key-value存储系统。
- 应用场景：互联网企业中经常要存储各种较大的相对静态的数据，例如图片、视频、音乐等类型的文件，分布式key-value系统就是用来解决这类存储需求的，单机key-value系统是整个系统的基础。
- 数据规模:单机存储 1000万条记录， 平均大小100K, 单机存储1T数据, 单条最大长度5MB。
- key-value-system目前提供C形式的共享库。程序员可以#include <kvs.h>来使用put, get, delete接口。理论上，系统运行时内存345MB + buffer模块内存。buffer模块最小内存由宏定义MINIMUN_BUFFER_SIZE指定。

## 安装

下载： 

```shell
git clone https://github.com/WadeLeng/key-value-system.git
```

进入key-value-system目录 

```shell
~/key-value-system$ sudo make all
```

## 使用

安装完系统之后，可以通过C语音的共享库在本机使用。
在程序中`#include <kvs.h>`
通过填充KVS_ENV参数进行初始化设定。通过put, get, delete接口进行操作。
编译方式（gcc 4.6.1）：加参数

```shell 
–lkvs -lpthread ~/key-value-system$gcc test_kvs.c –o test –lkvs -lpthread
```

## 接口详细说明

```c
KVS_ENV参数：
init_type:INIT_TYPE_CREATE //系统重新创建。
INIT_TYPE_LOAD //载入IMAGE_FILE，继续上次退出前状态运行。
disk_file_path //value存放的大文件路径。
IMAGE_file_path //index在内存中的镜像。下次系统启动可以载入。
log_file_path //log信息输出文件。
buffer_sleep_time //buffer_lookout线程刷新间隔时间，数据落地条件之一。
buffer_horizon_size //buffer_lookout线程中数据包大小，数据落地条件之一。
buffer_size //buffer模块在内存中的大小。
```

接口说明：

```c
/*系统初始化与退出接口，初始化前先填充参数KVS_ENV*/ int kv_init(const KVS_ENV* kvs); int kv_exit();
/*put/get/delete操作，用户调用之前需要先申请value的空间，系统背景value最大值是5MB*/ int kv_put(const char* key, int key_size, const char* value, int value_size); int kv_get(const char* key, int key_size, char* buf, int* buf_size); int kv_delete(const char* key, int key_size);
```