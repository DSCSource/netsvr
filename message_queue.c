#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "netsvr.h"
#include "netsvr_log.h"
//#include "spinlock.h"
#include "message_queue.h"

#define DEF_QUEUE_SIZE 128   //每次申请的消息块数量
#define MAX_GLOBAL_MQ  0x10000

#define MQ_IN_GLOBAL 1   
#define MQ_OVERLOAD 102400    //一条消息队列超载的消息块数量
#if 0
typedef int (*netsvr_callback)(struct netsvr_message *msg);
typedef int (*message_callback)(struct msg_queue *q);
struct msg_queue {
  struct spinlock lock;
  uint32_t handle;
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
#endif
struct global_queue {
  struct msg_queue *head;
  struct msg_queue *tail;
  struct spinlock lock;
};

static struct global_queue *g_queue=NULL;

void netsvr_globalmq_push(struct msg_queue *queue) 
{
  struct global_queue *q= g_queue;

  SPIN_LOCK(q)
  assert(queue->next == NULL);
  if(q->tail) {
    q->tail->next = queue;
    q->tail = queue;
  } else {
    q->head = q->tail = queue;
  }
  SPIN_UNLOCK(q)
}

struct msg_queue *netsvr_globalmq_pop() 
{
  struct global_queue *q = g_queue;

  SPIN_LOCK(q)
  struct msg_queue *mq = q->head;
  if(mq) {
    q->head = mq->next;
    if(q->head == NULL) {
      assert(mq == q->tail);
      q->tail = NULL;
    }
    mq->next = NULL;
  }
  SPIN_UNLOCK(q)

  return mq;
}

struct msg_queue *netsvr_mq_create(uint32_t handle) 
{
  struct msg_queue *q = netsvr_malloc(sizeof(*q));
  q->handle = handle;
  q->cap = DEF_QUEUE_SIZE;
  SPIN_INIT(q)
  // When the queue is create (always between service create and service init) ,
  // set in_global flag to avoid push it to global queue .
  // If the service init success, skynet_context_new will call skynet_mq_push to push it to global queue.
  q->isfull=0;
  q->in_global = 0;//MQ_IN_GLOBAL;
  q->release = 0;
  q->overload = 0;
  q->overload_threshold = MQ_OVERLOAD;
  q->alloc_queue = NULL;//netsvr_malloc(sizeof(struct netsvr_message) *q->cap);
  q->free_queue = NULL;
  /*int i;
  for(i=0;i< q->cap;i++) {
    q->alloc_queue[i].next=q->free_queue;
    q->free_queue=&q->alloc_queue[i];
  }*/
  q->queue_head = NULL;//skynet_malloc(sizeof(struct svr_message) * q->cap);
  q->queue_tail = NULL;
  q->next = NULL;
  return q;
}
void netsvr_mq_release(struct msg_queue *q) 
{
  assert(q->next == NULL);
  SPIN_DESTROY(q)
  struct netsvr_message *msg = NULL;
  do{
    msg = netsvr_mq_pop(q);
    if(msg != NULL && msg->data != NULL){
      netsvr_free(msg->data);
    }
  }while(msg != NULL);
  struct netsvr_message *head = q->alloc_queue;
  while(head != NULL){
    q->alloc_queue = head->next;      
    netsvr_free(head);
    head = q->alloc_queue;
  }
  netsvr_free(q);
}
uint32_t netsvr_mq_handle(struct msg_queue *q) 
{
  return q->handle;
}
struct netsvr_message *netsvr_mq_pop(struct msg_queue *q)
{
  struct netsvr_message *msg = NULL;
  SPIN_LOCK(q)
  if(q->queue_head == NULL) {
    q->in_global = 0;
  }else
  {
    msg = q->queue_head;
    q->queue_head = msg->next;
    q->overload --;
    if(q->queue_head == NULL){
      assert(msg == q->queue_tail);
      q->queue_tail = NULL;
    }
  }
  SPIN_UNLOCK(q)
  return msg;
}
static void expand_queue(struct msg_queue *q)
{
  int i;
  struct netsvr_message *node_pool = netsvr_malloc(sizeof(struct netsvr_message)*q->cap);
  for(i=0;i< q->cap;i++) {
    node_pool[i].next=q->free_queue;
    q->free_queue=&node_pool[i];
  }
  node_pool->next = q->alloc_queue;
  q->alloc_queue =  node_pool;
}
static struct netsvr_message *alloc_queue_message(struct msg_queue *q)
{
  struct netsvr_message *msg = NULL;
  if(q->free_queue == NULL) {
    expand_queue(q);
  }
  msg = q->free_queue;
  q->free_queue = msg->next;
  memset(msg,0,sizeof(struct netsvr_message));
  msg->next = NULL;

  return msg;
}
void free_queue_message(struct msg_queue *q, struct netsvr_message *message)
{
  if(message==NULL) {
    return;
  }
  SPIN_LOCK(q)
  message->next=q->free_queue;
  q->free_queue=message;
  SPIN_UNLOCK(q)
}
int netsvr_mq_push(struct msg_queue *q, uint32_t source, int session, void *data, size_t sz)
{
  struct netsvr_message *msg = NULL;
  SPIN_LOCK(q)
#if 0
  q->queue[q->tail] = *message; // 赋值操作
  if (++ q->tail >= q->cap) {  // 循环消息队列操作
    q->tail = 0;
  }
#endif
  msg = alloc_queue_message(q);
  printf("alloc message addr=%x\n",msg);
  if(msg==NULL){    //内存被消耗完
    netsvr_logout(NETSVR_IMPORT,"alloc_queue_message ERROR !\n");
    SPIN_UNLOCK(q)
    return ALLOC_MEMORY_ERROR;
  }
  msg->source = source;
  msg->session = session;
  msg->data = data;
  msg->sz = sz;
  //需要进行先来后到操作--------------------
  if(q->queue_head == NULL)
  {
    assert(q->queue_tail == NULL);
    q->queue_head = msg;
    q->queue_tail = msg;
  }else
  {
    q->queue_tail->next = msg;
    q->queue_tail = msg;
  }
  //msg->next = q->queue;
  //q->queue = msg;

  if (q->in_global == 0) {  // 之前消息队列中没有消息，将子消息队列添加到全局消息队列中
    q->in_global = MQ_IN_GLOBAL;
    netsvr_globalmq_push(q);
  }
  q->overload ++;
  netsvr_logout(NETSVR_IMPORT,"queue size = %d !\n",q->overload);
  if(q->overload >= q->overload_threshold){
    SPIN_UNLOCK(q)
    q->isfull = 1;
    netsvr_logout(NETSVR_IMPORT,"queue FULL !\n");
    return QUEUE_OVER_LOAD;
  }
  SPIN_UNLOCK(q)
  return PUSH_MESSAGE_SUCCESS;
}
int netsvr_mq_length(struct msg_queue *q)
{
  return q->overload;
}
void netsvr_mq_init()
{
  struct global_queue *q = netsvr_malloc(sizeof(struct global_queue));
  memset(q,0,sizeof(struct global_queue));
  SPIN_INIT(q);
  g_queue=q;
}

