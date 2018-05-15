#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <signal.h>

#include "netsvr.h"
#include "netsvr_timer.h"
#include "netsvr_log.h"
#include "message_queue.h"
#include "netsvr_worker.h"

#define MAX_TIMER_SIZE 16
static pthread_t timer_service_thread_id=0;
static pthread_mutex_t wait_startup_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_startup_cond = PTHREAD_COND_INITIALIZER;

struct Timer_Node {
  int32_t source;  //定时器所属服务
  int32_t state;   //定时器当前状态(开/关)
  int32_t loop;    //是超时后循环发送超时指令，还是超时触发一次。
  int32_t sec;     //定时器设置超时秒数
  int32_t current_sec; //定时器当前所走时间
  timeout timeout_cb;  //定时器超时回调
  struct Timer_Node *next;
};

static pthread_mutex_t  timer_thread_lock;
struct Timer_Node alloc_timer[MAX_TIMER_SIZE];
struct Timer_Node *free_timer=NULL;
struct Timer_Node *used_timer=NULL;

/*初始化节点数组*/
static void startup_alloc_pool()
{
  int i;
  for(i=0;i<MAX_TIMER_SIZE;i++) {
    alloc_timer[i].next=free_timer;
    free_timer=&alloc_timer[i];
  }
}
/*分配节点*/
static struct Timer_Node *alloc_timer_node()
{
  struct Timer_Node*node=NULL;

  if(free_timer==NULL) {
    return NULL;
  }
  node=free_timer;
  free_timer=node->next;
  memset(node,0,sizeof(struct Timer_Node));

  return node;
}
/*释放节点，并将节点回收*/
static void free_timer_node(struct Timer_Node *node)
{
  if(node==NULL) {
    return;
  }
  node->next=free_timer;
  free_timer=node;
}

/*创建定时器节点*/
struct Timer_Node *create_timer(int32_t handle,timeout cb)
{
  struct Timer_Node*node=NULL;
  node = alloc_timer_node();
  if(node == NULL){
    return NULL;
  }
  node->source = handle;
  node->timeout_cb = cb;
  node->current_sec=0;
  node->state=0;

  return node;
}
/*开启定时器*/
void start_timer(struct Timer_Node *node,int32_t loop,int32_t sec)
{
  node->loop=loop;
  node->sec=sec;
  node->state=1;
  pthread_mutex_lock(&timer_thread_lock);
  node->next=used_timer;
  used_timer = node;
  pthread_mutex_unlock(&timer_thread_lock);
}
/*停止定时器*/
void stop_timer(struct Timer_Node *node)
{
  struct Timer_Node **walk=&used_timer;
  struct Timer_Node *fnode=NULL;
  pthread_mutex_lock(&timer_thread_lock);
  while((*walk))
  {
    if((*walk) ==  node){
       fnode = (*walk);
       (*walk) = node->next;
       break;
    }
    walk = &(*walk)->next;
  }
  pthread_mutex_unlock(&timer_thread_lock);
}
/*释放定时器节点*/
void close_timer(struct Timer_Node *node)
{
  struct Timer_Node **walk=&used_timer;
  struct Timer_Node *fnode=NULL;
  pthread_mutex_lock(&timer_thread_lock);
  while((*walk))
  {
    if((*walk) == node){
       fnode = (*walk);
       (*walk) = fnode->next;
       free_timer_node(fnode);
       break;
    }
    walk = &(*walk)->next;
  }
  pthread_mutex_unlock(&timer_thread_lock);
}

static void timer_update()
{
  struct Timer_Node **walk=&used_timer;
  struct Timer_Node *fnode=NULL;
  pthread_mutex_lock(&timer_thread_lock);
  while((*walk))
  {
    (*walk)->current_sec++;
    if((*walk)->current_sec/40 >= (*walk)->sec)
    {
      (*walk)->current_sec=0;
      (*walk)->timeout_cb();
      if((*walk)->loop == 0){
         fnode = (*walk);
         (*walk) = fnode->next;
          continue;
      }
    }
    walk = &(*walk)->next;
  }
  pthread_mutex_unlock(&timer_thread_lock);
}

static int SIG = 0;

static void handle_hup(int signal) 
{
  if (signal == SIGHUP) {
    SIG = 1;
  }
}
static void signal_hup() 
{
  /* make log file reopen*/
  struct netsvr_message smsg;
  smsg.source = 0;
  smsg.session = 0;
  smsg.data = NULL;
  smsg.sz = (size_t)PTYPE_SYSTEM << MESSAGE_TYPE_SHIFT;
  //skynet_context_push(logger, &smsg);
}

static void *timer_service_thread(void *p) 
{
  pthread_mutex_lock(&wait_startup_lock);
  pthread_cond_broadcast(&wait_startup_cond);
  pthread_mutex_unlock(&wait_startup_lock);
  for (;;) {
    timer_update();
    wakeup();
    usleep(25000);   //休眠25毫秒
    if (SIG) {
      signal_hup();  //截取到程序退出信号，则往日志服务器写入日志
      SIG = 0;
    }
  }
  return NULL;
}


int start_timer_service()
{
  // register SIGHUP for log file reopen
  struct sigaction sa;
  sa.sa_handler = &handle_hup;
  sa.sa_flags = SA_RESTART;
  sigfillset(&sa.sa_mask);
  sigaction(SIGHUP, &sa, NULL);

  pthread_mutex_init(&timer_thread_lock,NULL);
  if(pthread_create(&timer_service_thread_id, NULL, timer_service_thread, NULL) != 0)
  {
    timer_service_thread_id=0;
    netsvr_logout(NETSVR_ERR,"create listen pthread failed!\n");
    return -1;
  }
  pthread_mutex_lock(&wait_startup_lock);
  pthread_cond_wait(&wait_startup_cond,&wait_startup_lock);
  pthread_mutex_unlock(&wait_startup_lock);
  return 0;
}

void wait_for_timer_service_exit()
{
  if(timer_service_thread_id != 0){
    pthread_join(timer_service_thread_id, NULL);
    timer_service_thread_id = 0;
  }
}
