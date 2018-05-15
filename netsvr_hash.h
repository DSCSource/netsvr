#ifndef __NETSVR_HASH_H__
#define __NETSVR_HASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include "spinlock.h"

#define DEFAULT_TABLE_SIZE      512

struct hash_value {
  union{
    uint32_t k_num;
    char k_char[32];       //最大支持32个字节的key值长度
  }key;
  uint32_t hashkey;
  void * data;             //数据存储位置
  struct hash_value *link; //同一个hash值的数据存储链表
};
typedef struct hash_table {
  struct hash_value **hash;
  struct spinlock lock;
  uint32_t nuse;   //hashtab中已插入的数据条数
  uint32_t size;   //当前hashtab的大小。
}hash_tab;

void hash_create(hash_tab *tab, uint32_t size);
//void hash_resize(hash_tab *tab, uint32_t newsize);
struct hash_value *hash_getvalue(hash_tab *tab, const char *key);
struct hash_value *hash_insert(hash_tab *tab, const char *key, void *data);
int hash_remove(hash_tab *tab, const char *key);
void hash_destroy(hash_tab *tab);


#ifdef __cplusplus
}
#endif



#endif
