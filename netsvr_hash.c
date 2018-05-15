#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "netsvr.h"
#include "netsvr_log.h"
#include "netsvr_hash.h"

#define LUAI_HASHLIMIT          5
//#define DEFAULT_TABLE_SIZE      512
#define check_exp(c,e)          (assert(c),(e))
#define lmod(s,size) \
        (check_exp((size&(size-1))==0, (cast(int, (s) & ((size)-1)))))
#if 0
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
  uint32_t nuse;   //hashtab中已插入的数据条数
  uint32_t size;   //当前hashtab的大小。
}hash_tab;
#endif 
static unsigned int luaS_hash (const char *str, size_t l, unsigned int seed) {
  unsigned int h = seed ^ cast(unsigned int, l);
  size_t step = (l >> LUAI_HASHLIMIT) + 1;
  for (; l >= step; l -= step)
    h ^= ((h<<5) + (h>>2) + cast_byte(str[l - 1]));
  return h;
}

//创建：       hash_create(hash_tab *tab, uint_t size)
void hash_create(hash_tab *tab, uint32_t size)
{
  int i=0;
  hash_tab *tb = tab;
  SPIN_INIT(tb)
  tb->hash = netsvr_malloc(sizeof(struct hash_value *)*size);
  for(i=0; i<size; i++)
  {
     tb->hash[i] = NULL;//netsvr_malloc(sizeof(struct hash_value));
  }
  tb->nuse = 0;
  tb->size = size;
}
//修改tab大小：hash_resize(hash_tab *tab, uint_t newsize)
static void hash_resize(hash_tab *tab, uint32_t newsize)
{
  int i=0;
  hash_tab *tb = tab;
  struct hash_value **newhash=NULL;
  //如果是增大tab大小，则先realloc重新分配tab的大小
  if (newsize > tb->size) {  /* grow table if needed */
    newhash = netsvr_realloc(tb->hash, sizeof(struct hash_value *)*newsize);
    if(!newhash){
      return;
    }
    tb->hash = newhash;
    for (i = tb->size; i < newsize; i++)
      tb->hash[i] = NULL;
  }
  //在原有的size根据hashkey重新计算（与新的newsize大小取模）数据在tab中的位置。
  unsigned int h=0;
  struct hash_value *p=NULL;
  struct hash_value *hnext=NULL;
  for (i = 0; i < tb->size; i++) {  /* rehash */
    p = tb->hash[i];
    tb->hash[i] = NULL;
    while (p) {  /* for each node in the list */
      hnext = p->link;  /* save next */
      h = lmod(p->hashkey, newsize);  /* new position */
      p->link = tb->hash[h];  /* chain it */
      tb->hash[h] = p;
      p = hnext;
    }
  }
  //如果是缩小tab大小，则计算数据在tab中的位置以后，再realloc重新分配tab的大小
  if (newsize < tb->size) {  /* shrink table if needed */
    /* vanishing slice should be empty */
    assert(tb->hash[newsize] == NULL && tb->hash[tb->size - 1] == NULL);
    //luaM_reallocvector(L, tb->hash, tb->size, newsize, TString *);
    newhash = netsvr_realloc(tb->hash, sizeof(struct hash_value *)*newsize);
    if(!newhash){
      return;
    }
    tb->hash = newhash;
  }
  tb->size = newsize;
}
struct hash_value *for_hash(hash_tab *tab)
{
}
//查找:        hash_getvalue(hash_tab *tab, char *key)
struct hash_value *hash_getvalue(hash_tab *tab, const char *key)
{
  hash_tab *tb = tab;
  unsigned int h = luaS_hash(key, strlen(key), 0);
  SPIN_LOCK(tb)
  struct hash_value **walk = &tb->hash[lmod(h, tb->size)];

  while((*walk)!=NULL) {
    if(strcmp((*walk)->key.k_char, key) == 0) {
      SPIN_UNLOCK(tb)
      return (*walk);
    }
    walk=&(*walk)->link;
  }
  SPIN_UNLOCK(tb)
  return NULL;
}
//插入：       hash_insert(hash_tab *tab, char *key, void *data)
struct hash_value *hash_insert(hash_tab *tab, const char *key, void *data)
{
  hash_tab *tb = tab;
  struct hash_value *value=NULL;
  unsigned int h = luaS_hash(key, strlen(key), 0);
  value=hash_getvalue(tb, key);
  if(value) {//由于data自己管理释放，所以只要返回value就可以了。
    //value->data=data;
    return value;
  }
  // lookup global state of this L first
  SPIN_LOCK(tb)
  struct hash_value **list = &tb->hash[lmod(h, tb->size)];
  if (tb->nuse >= tb->size && tb->size <= INT_MAX/2) {
    hash_resize(tb, tb->size * 2);
    list = &tb->hash[lmod(h, tb->size)];  /* recompute with new size */
  }
  struct hash_value *walk = netsvr_malloc(sizeof(struct hash_value));
  walk->hashkey = h;
  strcpy(walk->key.k_char, key);
  walk->data = data;
  walk->link = (*list);
  (*list) = walk;
  tb->nuse++;
  SPIN_UNLOCK(tb)
  netsvr_logout(NETSVR_INFO, "Add hash node to table, node size:%d\n",tb->nuse);
  return walk;
}
//删除：       hash_remove(hash_tab *tab, char *key)   
int hash_remove(hash_tab *tab, const char *key)
{
  hash_tab *tb = tab;
  struct hash_value *node = NULL;
  unsigned int h = luaS_hash(key, strlen(key), 0);
  SPIN_LOCK(tb)
  struct hash_value **list = &tb->hash[lmod(h, tb->size)];
  //struct hash_value *value=hash_getvalue(tb, key);
  
  while((*list)!=NULL) {
    if(strcmp((*list)->key.k_char, key) == 0) {
      node = (*list);
      if(node->data != NULL) {
        SPIN_UNLOCK(tb)
        return 0;
      }
      (*list) = node->link;
      netsvr_free(node);
      break;
    }
    list=&(*list)->link;
  }
  tb->nuse--;
  if(tb->size>DEFAULT_TABLE_SIZE && tb->nuse < (tb->size/4)) {
    hash_resize(tb, tb->size/2);
  }
  SPIN_UNLOCK(tb)
  netsvr_logout(NETSVR_INFO, "del hash node to table, node size:%d\n",tb->nuse);
  return 1;
}
//清除：       hash_destroy(hash_tab *tab)
void hash_destroy(hash_tab *tab)
{
  int i=0;
  hash_tab *tb = tab;
  struct hash_value *node = NULL;
  struct hash_value **p = NULL;
  SPIN_LOCK(tb)
  for (i = 0; i < tb->size; i++) {  /* rehash */
    p = &tb->hash[i];
    tb->hash[i] = NULL;
    while ((*p)) {  /* for each node in the list */
      node = (*p);
      netsvr_free(node);
      p = &(*p)->link;
    }
  }
  netsvr_free(tb->hash);
  SPIN_UNLOCK(tb)
  SPIN_DESTROY(tb)
}




