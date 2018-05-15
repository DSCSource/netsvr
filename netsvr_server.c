#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include "netsvr.h"
#include "rw_socket.h"
#include "netsvr_server.h"
#include "netsvr_hash.h"
#include "message_queue.h"
#include "netsvr_worker.h"

#define RECV_BUFFER_SIZE 2048
#define SEND_BUFFER_SIZE 65536
#define USED_IP_LEN 16
#define MAX_THREAD 4
#define MAX_EVENTS 64
#define ALLOC_NODE_SIZE 64
#define SOCKET_ERROR -1

static int keepAlive = 1;       /*设定KeepAlive*/
static int keepIdle = 30;        /*首次探测开始前的tcp无数据收发空闲时间*/
static int keepInterval = 5;   /*每次探测的间隔时间*/
static int keepCount = 2;       /*探测次数*/

static pthread_mutex_t wait_startup_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_startup_cond=PTHREAD_COND_INITIALIZER;

struct SERVER_TCP_NODE {
  int  serverid;      //服务器编号
  int  sockfd;        //服务器连接过来的socket
  int  resockfd;      //当前服务器回连的socket
  int  load_client;   //该IP服务器负载数量
  int  load_max;      //当前服务器负载数量
  int  port;          //端口
  char ip[USED_IP_LEN]; //IP地址
  struct SERVER_TCP_NODE * next;
};

static pthread_mutex_t pool_lock;
static struct SERVER_TCP_NODE *alloc_list=NULL;
static struct SERVER_TCP_NODE *free_list=NULL;                  /*机顶盒尾节点指针*/
/*初始化节点数组*/
static void startup_alloc_pool()
{
  int i;
  struct SERVER_TCP_NODE *node_pool = netsvr_malloc(ALLOC_NODE_SIZE*sizeof(struct SERVER_TCP_NODE));
  for(i=0;i<ALLOC_NODE_SIZE;i++) {
    node_pool[i].next=free_list;
    free_list=&node_pool[i];
  }
  node_pool->next = alloc_list;
  alloc_list =  node_pool;
}
/*分配节点*/
static struct SERVER_TCP_NODE *alloc_server_node()
{
  struct SERVER_TCP_NODE*node=NULL;
  pthread_mutex_lock(&pool_lock);
  if(free_list==NULL) {
    startup_alloc_pool();
  }
  node=free_list;
  free_list=node->next;
  memset(node,0,sizeof(struct SERVER_TCP_NODE));
  pthread_mutex_unlock(&pool_lock);
  return node;
}
/*释放节点，并将节点回收*/
static void free_server_node(struct SERVER_TCP_NODE *node)
{
  if(node==NULL) {
    return;
  }
  pthread_mutex_lock(&pool_lock);
  node->next=free_list;
  free_list=node;
  pthread_mutex_unlock(&pool_lock);
}

/*服务线程结构*/
struct SERVER_SERVICE_THREAD {
  pthread_t pid;                       /*线程ID*/
  int  epfd;
  int  client_num;
  hash_tab *tab_client;
  pthread_mutex_t thread_lock;         /*线程锁*/
  char recv_buf[RECV_BUFFER_SIZE];     /*数据接收缓存区*/
  char send_buf[SEND_BUFFER_SIZE];     /*数据发送缓存区*/
  struct epoll_event events[MAX_EVENTS];
  struct msg_queue *queue;
};

static int process_server_event(struct netsvr_message *msg)
{
  printf("msg:%s\n",msg->data);

  return 0;
}

/*客户端服务数据收发线程*/
static void*server_service_thread(void*param)
{
  int len=0,sockfd=0;
  int nfds,i,b_disconnect;
  struct epoll_event ev;
  struct SERVER_TCP_NODE**walk=NULL;
  struct SERVER_SERVICE_THREAD*thread_node=(struct SERVER_SERVICE_THREAD*)param;

/*reconnect:*/
  pthread_mutex_lock(&wait_startup_lock);
  pthread_cond_broadcast(&wait_startup_cond);
  pthread_mutex_unlock(&wait_startup_lock);

  while(1) {
    nfds = epoll_wait(thread_node->epfd, thread_node->events, MAX_EVENTS, -1);
    if (nfds == -1) {
      perror("epoll_pwait");
      exit(EXIT_FAILURE);
    }
    for(i=0;i<nfds;i++)
    {
      if( thread_node->events[i].events&EPOLLIN ) /*接收到数据，读socket*/
      {
        netsvr_logout(NETSVR_INFO, "read server event coming!\n");
        walk = (struct SERVER_TCP_NODE**)&thread_node->events[i].data.ptr;
        if((sockfd = (*walk)->sockfd) < 0) {
          continue;
        }
        len = read_pack_data(sockfd, thread_node->recv_buf, RECV_BUFFER_SIZE);
        if(len==CONNECT_ERR) {   /*读取句柄数据出错*/
          netsvr_logout(NETSVR_WARN,"server %s: disconnect Server!\n",(*walk)->ip);
          thread_node->client_num--;
          ev.events = EPOLLIN|EPOLLET; //监听读状态同时设置ET模式
          ev.data.fd = sockfd;
          epoll_ctl(thread_node->epfd,EPOLL_CTL_DEL,sockfd, &ev); //注册epoll事件
          close(sockfd);
          sockfd=0;
          struct SERVER_TCP_NODE*node=(*walk);
          struct hash_value *value = hash_getvalue(thread_node->tab_client, node->ip);
          if(value) {
            value->data=NULL;
            if(hash_remove(thread_node->tab_client, node->ip)==0){
              netsvr_logout(NETSVR_WARN,"remove tab node failed !\n");
            }
          }
          (*walk)=node->next; // remove
          node->next=0;
          free_server_node(node);
          continue;
        }
        else if(len==OUT_OF_MEM) {   /*接收数组溢出*/
          netsvr_logout(NETSVR_WARN,"recv length more then recv buf size 2048!\n");
          continue;
        }
        else if(len==0) {
          continue;
        }
        /*printf("%s\n",thread_node->recv_buf);*/
        char *data = NULL;
        data = netsvr_strdup(thread_node->recv_buf);
        int ret=netsvr_mq_push(thread_node->queue, thread_node->pid, 0, data, len);
        if(ret==QUEUE_OVER_LOAD){
          thread_node->queue = netsvr_mq_create((int)thread_node->pid);
          thread_node->queue->callback = process_server_event;
          netsvr_logout(NETSVR_IMPORT,"queue FULL !\n");
        }else if(ret==ALLOC_MEMORY_ERROR)
        {
          netsvr_logout(NETSVR_IMPORT,"alloc_queue_message ERROR !\n");
        }
        netsvr_logout(NETSVR_IMPORT,"push message Success !\n");
        if(exist_worker_exex()){
          wakeup();
        }
      }
      else if(thread_node->events[i].events&EPOLLOUT) /*有数据待发送，写socket*/
      {
        netsvr_logout(NETSVR_INFO, "write to server event coming!\n");
        walk = (struct SERVER_TCP_NODE**)&thread_node->events[i].data.ptr;
        if((sockfd = (*walk)->sockfd) < 0) {
          continue;
        }
        if((sockfd = thread_node->events[i].data.fd) < 0) {
          continue;
        }
        /*修改标识符，等待下一个循环时接收数>据*/
        /*ev.data.fd = sockfd;*/     /*md为自定义类型，添加数据*/
        /*ev.events=EPOLLIN|EPOLLET;
        epoll_ctl(thread_node->epfd,EPOLL_CTL_MOD,sockfd,&ev);*/
      }
      else
      {
        netsvr_logout(NETSVR_IMPORT, "unkown event coming !\n");
        /*其他情况的处理*/
      }
    }
  }
  close(thread_node->epfd);
}

static int init_server_service_thread(struct SERVER_SERVICE_THREAD*server_service)
{
  memset(server_service,0,sizeof(struct SERVER_SERVICE_THREAD));
  pthread_mutex_init(&server_service->thread_lock,NULL);
  server_service->epfd = epoll_create(MAX_EVENTS);
  if(server_service->epfd == -1) {
    netsvr_logout(NETSVR_ERR, "create pthread epoll fd failed!\n");
    return -1;
  }
  server_service->tab_client = netsvr_malloc(sizeof(hash_tab));
  hash_create(server_service->tab_client, DEFAULT_TABLE_SIZE);

  if(pthread_create(&server_service->pid,NULL,server_service_thread,server_service)<0){
    netsvr_logout(NETSVR_WARN, "create pthread failed!\n");
    return -1;
  }
  server_service->queue = netsvr_mq_create((int)server_service->pid);
  server_service->queue->callback = process_server_event;

  pthread_mutex_lock(&wait_startup_lock);
  pthread_cond_wait(&wait_startup_cond,&wait_startup_lock);
  pthread_mutex_unlock(&wait_startup_lock);
  return 0;
}

static struct SERVER_SERVICE_THREAD service_thread[MAX_THREAD];
static int SERVER_SERVICE_NUM=0;
static uint64_t mount_index=0;
static int config_server_socket(int client)
{
  if(setsockopt(client,SOL_SOCKET,SO_KEEPALIVE,(void*)&keepAlive,sizeof(keepAlive))==SOCKET_ERROR) {
    netsvr_logout(NETSVR_WARN,"Call setsockopt error, errno is %d/n", errno);
  }
  if(setsockopt(client,SOL_TCP,TCP_KEEPIDLE,(void*)&keepIdle,sizeof(keepIdle))==SOCKET_ERROR) {
    netsvr_logout(NETSVR_WARN,"Call setsockopt error, errno is %d/n", errno);
  }
  if(setsockopt(client,SOL_TCP,TCP_KEEPINTVL,(void*)&keepInterval,sizeof(keepInterval))==SOCKET_ERROR) {
    netsvr_logout(NETSVR_WARN,"Call setsockopt error, errno is %d/n", errno);
  }
  if(setsockopt(client,SOL_TCP,TCP_KEEPCNT,(void*)&keepCount,sizeof(keepCount))==SOCKET_ERROR) {
    netsvr_logout(NETSVR_WARN,"Call setsockopt error, errno is %d/n", errno);
  }
  return 0;
}

static int reconnect_to_other_server()
{
  int sockfd=0;
  
}

int add_server_to_server_service_pool(int clientfd, char *ip, int port)
{
  struct epoll_event ev;
  struct SERVER_TCP_NODE*node=NULL;
  struct SERVER_SERVICE_THREAD*thread_node=NULL;
  if(config_server_socket(clientfd)<0) {
    netsvr_logout(NETSVR_ERR,"config socket error!\n");
    return -1;
  }
  node=alloc_server_node();
  if(node==NULL) {
    netsvr_logout(NETSVR_ERR,"TCP NODE cannot alloc TCP_NODE node,please check!\n");
    return -1;
  }
  node->sockfd=clientfd;
  strcpy(node->ip,ip);
  node->port=port;

  int i=0;
  mount_index=0;
  for(i=1; i<SERVER_SERVICE_NUM; i++)
  {
    if(service_thread[mount_index].client_num > service_thread[i].client_num)
    {
      mount_index=i;
    }
  }
  thread_node=&service_thread[mount_index];
  thread_node->client_num++;

  printf("node memory addr : %x thread index = %d\n",node,mount_index);
  ev.events = EPOLLIN|EPOLLET; //监听读状态同时设置ET模式
  ev.data.ptr = (void *)node;
  pthread_mutex_lock(&thread_node->thread_lock);
  epoll_ctl(thread_node->epfd, EPOLL_CTL_ADD, clientfd, &ev); //注册epoll事件
  //node->next=thread_node->client_list;
  //thread_node->client_list=node;
  hash_insert(thread_node->tab_client, ip, node);
  pthread_mutex_unlock(&thread_node->thread_lock);
  return 0;
}

int start_server_service(int thread_num)
{
  int i;
  if(thread_num<=0||thread_num>MAX_THREAD) {
    thread_num=4;
  }
  SERVER_SERVICE_NUM=0;
  pthread_mutex_init(&pool_lock, NULL);
  startup_alloc_pool();

  for(i=0;i<thread_num;i++) {
    if(init_server_service_thread(&service_thread[SERVER_SERVICE_NUM])<0) {
      netsvr_logout(NETSVR_WARN,"start server service thread index %d failed !\n",i);
      break;
    }
    else {
      SERVER_SERVICE_NUM++;
    }
  }
  if(SERVER_SERVICE_NUM<=0) {
    netsvr_logout(NETSVR_ERR,"start server service thread failed !\n");
    return -1;
  }
  return 0;
}

void wait_for_server_service_exit()
{
  int i;
  for(i=0;i<SERVER_SERVICE_NUM;i++) {
    pthread_join(service_thread[i].pid,NULL);
  }
}
