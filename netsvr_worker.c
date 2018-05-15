#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "netsvr.h"
#include "netsvr_log.h"
#include "message_queue.h"

#define MAX_WORKER_THREAD 32

static pthread_mutex_t wait_startup_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_startup_cond=PTHREAD_COND_INITIALIZER;
static pthread_mutex_t wakeup_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wakeup_cond=PTHREAD_COND_INITIALIZER;
static uint16_t sleep_thread = 0;
static uint16_t worker_thread = 0;
static pthread_t pid[MAX_WORKER_THREAD];

static struct msg_queue *netsvr_context_message_dispatch(struct msg_queue *q, int weight) 
{
  if (q == NULL) {
    q = netsvr_globalmq_pop();   //获取消息队列中的消息
    if (q==NULL)                 //没有消息
      return NULL;
  }
/*
  if(q->callback == NULL){
    netsvr_mq_release(q);
    return netsvr_globalmq_pop();   //轮询下一个子消息队列
  }
*/
  int i,n=1,ret=0;
  struct netsvr_message *msg=NULL;
  for (i=0;i<n;i++) {  //取出当前服务的一个消息。
    msg = netsvr_mq_pop(q);
    if (msg == NULL) {   //当前服务中没有消息
      if(q->isfull==1) {
        netsvr_logout(NETSVR_IMPORT, "queue is full and msg is NUUL will release !\n");
        netsvr_mq_release(q);
      }
      return netsvr_globalmq_pop();//轮询下一个子消息队列
    } else if (i==0 && weight >= 0) {
      n = netsvr_mq_length(q);  //获取消息数量
      n >>= weight; //n = n>>weight;
    }

    if (q->callback == NULL) {
      netsvr_free(msg->data);
      free_queue_message(q, msg);
    } else {
      ret=q->callback(msg);
      if(ret==0){
        netsvr_free(msg->data);
        free_queue_message(q, msg);
      }
    }
  }
  //判断消息队列是否还挂载在服务线程，如果不在服务线程则释放掉该消息队列

  struct msg_queue *nq = netsvr_globalmq_pop();
  if (nq) {  //如果全局消息队列不为空，则开始处理另外一个子消息队列，将当前子消息队列加入队列尾，否则继续处理当前子消息队列
  // If global mq is not empty , push q back, and return next queue (nq)
  // Else (global mq is empty or block, don't push q back, and return q again (for next dispatch)
    netsvr_globalmq_push(q);
    q = nq;
  } 
  return q;
}

static void *thread_worker(void *param) {
  struct msg_queue * q = NULL;

  pthread_mutex_lock(&wait_startup_lock);
  pthread_cond_broadcast(&wait_startup_cond);
  pthread_mutex_unlock(&wait_startup_lock);
  int weight=0;
  while (1) {
    q = netsvr_context_message_dispatch(q, weight);
    if (q == NULL) {
      if (pthread_mutex_lock(&wakeup_lock) == 0) {
        ++ sleep_thread;   //休眠的工作线程数量
        //if (!m->quit)i
        //netsvr_logout(NETSVR_INFO, "lock worker thread = %d\n",sleep_thread);
        pthread_cond_wait(&wakeup_cond, &wakeup_lock);
        -- sleep_thread;
        if (pthread_mutex_unlock(&wakeup_lock)) {
          fprintf(stderr, "unlock mutex error");
        }
      }
    }
    //netsvr_logout(NETSVR_INFO, "sleep worker thread = %d\n",sleep_thread);
  }
  return NULL;
}

static int init_worker_service_thread(int index)
{
  if(pthread_create(&pid[index],NULL,thread_worker,NULL)<0){
    netsvr_logout(NETSVR_WARN, "create worker pthread failed!\n");
    return -1;
  }

  pthread_mutex_lock(&wait_startup_lock);
  pthread_cond_wait(&wait_startup_cond,&wait_startup_lock);
  pthread_mutex_unlock(&wait_startup_lock);
  return 0;
}

int exist_worker_exex() {
  return sleep_thread>=worker_thread;
}

void wakeup() {
  //6 8-7 一个工作线程专门用来处理socket的消息队列。每唤醒一个线程sleep减一。当sleep大于1
  if (sleep_thread >= 1) {  //m->count - busy空闲线程数量 值为1
    //signal sleep worker, "spurious wakeup" is harmless
    pthread_cond_signal(&wakeup_cond);
  }
}

int start_worker_service(int pthread)
{
  //最大支持32个工作线程
  if(pthread<=0 || pthread > MAX_WORKER_THREAD){
    pthread = MAX_WORKER_THREAD;
  }
  worker_thread=0;
  int i=0;
  for(i=0; i<pthread; i++)
  {
    if(init_worker_service_thread(worker_thread)<0) {
      netsvr_logout(NETSVR_WARN,"start worker service thread index %d failed !\n",i);
      break;
    }
    else {
       worker_thread++;
       netsvr_logout(NETSVR_INFO,"start worker num = %d !\n",worker_thread);
    }
  }
  netsvr_logout(NETSVR_INFO,"start worker num = %d !\n",worker_thread);
  if(worker_thread<=0) {
    netsvr_logout(NETSVR_ERR,"start worker service thread failed !\n");
    return -1;
  }
  return 0;
}

void wait_for_worker_service_exit()
{
  int i;
  for(i=0;i<worker_thread;i++) {
    pthread_join(pid[i],NULL);
  }
}
