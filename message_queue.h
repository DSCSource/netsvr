#ifndef NET_MESSAGE_QUEUE_H_
#define NET_MESSAGE_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include "spinlock.h"

struct netsvr_message {
  uint32_t source;
  int      session;
  void     *data;
  size_t   sz;
  struct netsvr_message *next;
};

typedef int (*netsvr_callback)(struct netsvr_message *msg);
typedef int (*message_callback)(void *q);
struct msg_queue {
  struct spinlock lock;
  uint32_t handle;
  int      isfull;
  int      cap;
  int      release;
  int      in_global;
  int      overload;
  int      overload_threshold;   //
  netsvr_callback  callback;
  message_callback queue_cb;
  struct netsvr_message *free_queue;
  struct netsvr_message *alloc_queue;  //save alloc message queues
  struct netsvr_message *queue_head;
  struct netsvr_message *queue_tail;
  struct msg_queue      *next;
};

#define MESSAGE_TYPE_MASK (SIZE_MAX >> 8)
#define MESSAGE_TYPE_SHIFT ((sizeof(size_t)-1) * 8)
#define PUSH_MESSAGE_SUCCESS 0
#define ALLOC_MEMORY_ERROR -1
#define QUEUE_OVER_LOAD    -2
//struct msg_queue;
void netsvr_globalmq_push(struct msg_queue *queue);
struct msg_queue *netsvr_globalmq_pop(void);
struct msg_queue *netsvr_mq_create(uint32_t handle);
void netsvr_mq_release(struct msg_queue *q);

//typedef void (*message_drop)(struct netsvr_message *, void *);

//void netsvr_mq_release(struct msg_queue *q, message_drop drop_func, void *ud);
uint32_t netsvr_mq_handle(struct msg_queue *);
struct netsvr_message *netsvr_mq_pop(struct msg_queue *q);
int netsvr_mq_push(struct msg_queue *q, uint32_t source, int session, void *data, size_t sz);
void free_queue_message(struct msg_queue *q, struct netsvr_message *message);
// return the length of message queue, for debug
int netsvr_mq_length(struct msg_queue *q);

void netsvr_mq_init();

#ifdef __cplusplus
}
#endif

#endif
