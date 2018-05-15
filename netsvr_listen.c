/*
 * ----------------------------服务监听模块（线程）----------------------
 * 1, 开启TCP服务, 并开启线程监听端口3361的连接, 采用select来检测服务监听端口是否有客户端连接.
 * 2, 接收客户端发送过来的认证消息, 如果不能认证通过则认为是未知连接, 直接断开不做处理. 否则将客户端套接字分配到各个对应的处理线程. 并回复客户端通过验证的消息.
 * 
 *   2018-03-16  dengsc
*/
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

#include "netsvr.h"
#include "rw_socket.h"
#include "netsvr_listen.h"
#include "netsvr_client.h"

static const char *CONNECTED_CLIENT="NetClient";    //
static const char *CONNECTED_SERVER="NetServer";    //
static const char *CONNECTED_SUCCESS="NetConnected";    //

#define USED_IP_LEN 16
#define USED_UID_LEN 32
#define SERVER_PORT 6331
#define LISTEN_RECV_SIZE 64

static pthread_t listen_service_thread_id = 0;
static pthread_mutex_t wait_startup_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wait_startup_cond = PTHREAD_COND_INITIALIZER;

enum SERVICE_TYPE {
  SERVICE_CLIENT=1,
  SERVICE_SERVER=2
};

struct CLIENT_NODE {
  int clientfd;
  int port;
  char ip[USED_IP_LEN];
  struct CLIENT_NODE *next;
};

#define MAX_LISTEN_SIZE 1024
static struct CLIENT_NODE alloc_list[MAX_LISTEN_SIZE];
static struct CLIENT_NODE *free_list=NULL;
static struct CLIENT_NODE *used_list=NULL;

/*初始化节点数组*/
static void startup_alloc_pool()
{
  int i;
  for(i=0;i<MAX_LISTEN_SIZE;i++) {
    alloc_list[i].next=free_list;
    free_list=&alloc_list[i];
  }
}
/*分配节点*/
static struct CLIENT_NODE *alloc_client_node()
{
  struct CLIENT_NODE*node=NULL;

  if(free_list==NULL) {
    return NULL;
  }
  node=free_list;
  free_list=node->next;
  memset(node,0,sizeof(struct CLIENT_NODE));

  return node;
}
/*释放节点，并将节点回收*/
static void free_client_node(struct CLIENT_NODE *node)
{
  if(node==NULL) {
    return;
  }
  node->next=free_list;
  free_list=node;
}

static int listen_valid_client(char *buf,char *uid,int *type)
{
  int check_len=0;
  int uid_len=0;
  char check_buf[USED_IP_LEN];

  memset(check_buf, '\0', USED_IP_LEN);
  memcpy(&check_len, buf, sizeof(int));
  if(check_len <= 0 || check_len>USED_IP_LEN) {
    return -1;
  }
  memcpy(check_buf, buf+sizeof(int), check_len);
  memcpy(&uid_len, buf+sizeof(int)+check_len, sizeof(int));
  if(uid_len<= 0 || uid_len>USED_UID_LEN) {
    return -1;
  }
  memcpy(uid, buf+2*sizeof(int)+check_len, uid_len);

  if(strcmp(check_buf,CONNECTED_CLIENT)==0) {  //
    *type=SERVICE_CLIENT;
    return 0;
  }
  else if(strcmp(check_buf, CONNECTED_SERVER)==0) { //
    *type=SERVICE_SERVER;
    return 0;
  }
  return -1;
}

static void *listen_service_thread(void *param)
{
  int sockfd=(int)param;
  int client_fd=0;
  int ret=0, i=0;
  struct timeval tv;
  struct sockaddr_in client_addr;
  fd_set set;
  socklen_t sock_len;
  char uid[USED_UID_LEN];
  char recv_buf[LISTEN_RECV_SIZE];
  int data_len=0;
  struct CLIENT_NODE *node=NULL;
reconnect:
  pthread_mutex_lock(&wait_startup_lock);
  pthread_cond_broadcast(&wait_startup_cond);
  pthread_mutex_unlock(&wait_startup_lock);

  while(1)
  {
    FD_ZERO(&set);
    FD_SET(sockfd,&set);
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    struct CLIENT_NODE **walk=&used_list;
    while((*walk) != NULL) {
      FD_SET((*walk)->clientfd, &set);
      walk = &(*walk)->next;
    }
    ret = select(FD_SETSIZE, &set, NULL, NULL, &tv);
    if(ret == -1) {
      netsvr_logout(NETSVR_ERR,"listen service select error !\n");
      sleep(1);
      continue;
    }else if(ret == 0){
      walk=&used_list;
      while((*walk) != NULL) {
        netsvr_logout(NETSVR_IMPORT,"listen service close unkown client ip:%s !\n",(*walk)->ip);
        node = (*walk);
        (*walk) = node->next;
        close(node->clientfd);
        node->clientfd=0;
        node->next=NULL;
        free_client_node(node);
      }
      continue;
    }
    if(FD_ISSET(sockfd, &set)) {
      netsvr_logout(NETSVR_INFO,"listen service recv client connected !\n");
      sock_len = sizeof(struct sockaddr);
      client_fd = accept(sockfd,(struct sockaddr *)(&client_addr),&sock_len);
      if(client_fd==-1) {
        netsvr_logout(NETSVR_INFO,"accetp an invalid sokcet!\n");
        continue;
      }
      node = alloc_client_node();
      if(node == NULL) {
        netsvr_logout(NETSVR_ERR,"listen service not alloc client node !\n");
        continue;
      }
      memset(node->ip,'\0', USED_IP_LEN);
      strcpy(node->ip,inet_ntoa(client_addr.sin_addr));
      node->port=ntohs(client_addr.sin_port);
      node->clientfd=client_fd;
      node->next = used_list;
      used_list = node;
      continue;
    }
    walk=&used_list;
    while((*walk) != NULL)
    {
      if(FD_ISSET((*walk)->clientfd, &set))
      {
        memset(recv_buf, '\0', LISTEN_RECV_SIZE);
        data_len = read_pack_data((*walk)->clientfd,recv_buf,LISTEN_RECV_SIZE);
        if(data_len<=0) {
          close((*walk)->clientfd);
          goto free_node;
        }
        int type=0;
        memset(uid, '\0', USED_UID_LEN);
        if(listen_valid_client(recv_buf,uid,&type)<0) {
          close((*walk)->clientfd);
          goto free_node;
        }
        memset(recv_buf, '\0', LISTEN_RECV_SIZE);
        data_len = strlen(CONNECTED_SUCCESS)+1;
        memcpy(recv_buf,&data_len,sizeof(int));
        memcpy(recv_buf+sizeof(int), CONNECTED_SUCCESS, data_len);
        data_len += 2*sizeof(int);
        int len=write_data((*walk)->clientfd, &data_len, (int)sizeof(int));
        if(len!=4) {
          netsvr_logout(NETSVR_INFO,"write socket error when in client_cerf send data len %d!!!\n",4);
          close((*walk)->clientfd);
          goto free_node;
        }
        len=write_data((*walk)->clientfd, recv_buf, data_len-4);
        if(len!=(data_len-4)) {
          netsvr_logout(NETSVR_INFO,"write socket error when in client_cerf send data length:%d!!!\n",data_len-4);
          close((*walk)->clientfd);
          goto free_node;
        }
        switch(type)
        {
        case SERVICE_CLIENT:
          if(add_client_to_client_service_pool((*walk)->clientfd,uid,(*walk)->ip,(*walk)->port)<0) {
            printf("add client to stb service failed !\n");
            close((*walk)->clientfd);
          }
          else {
            netsvr_logout(NETSVR_INFO,"an client connected Serer service uid:%s IP:%s port:%d!!!\n",uid,(*walk)->ip,(*walk)->port);
            //close(client_fd);
          }
          break;
        case SERVICE_SERVER:
          if(add_server_to_server_service_pool((*walk)->clientfd,(*walk)->ip,(*walk)->port)<0) {
            printf("add server to server service failed !\n");
            close((*walk)->clientfd);
          }
          else {
            printf("an server connected Serer service IP:%s port:%d!!!",(*walk)->ip,(*walk)->port);
            //close(client_fd);
          }
          break;
        }
free_node:
        node = (*walk);
        (*walk) = node->next;
        node->clientfd=0;
        node->next = NULL;
        free_client_node(node);
        continue;
      }
      walk = &(*walk)->next;
    }
  }
}

int start_listen_service()
{
  int sockfd = -1;
  int var=1, on=1;
  struct sockaddr_in server_addr;
  sockfd = socket(AF_INET,SOCK_STREAM,0);
  if(sockfd == -1) {
    netsvr_logout(NETSVR_ERR,"listen open socket error !\n");
    goto failed;
  }
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&var, 4);
 
  if(bind(sockfd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr)) == -1)
  {
    netsvr_logout(NETSVR_ERR,"listen bind listen socket error !\n");
    goto failed;
  }
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, 4);

  if(listen(sockfd, 1000) == -1)
  {
    netsvr_logout(NETSVR_ERR,"listen socket error !\n");
  }
  startup_alloc_pool();
  if(pthread_create(&listen_service_thread_id, NULL, listen_service_thread, (void*)sockfd) != 0)
  {
    listen_service_thread_id=0;
    netsvr_logout(NETSVR_ERR,"create listen pthread failed!\n");
    goto failed;
  }
  
  pthread_mutex_lock(&wait_startup_lock);
  pthread_cond_wait(&wait_startup_cond,&wait_startup_lock);
  pthread_mutex_unlock(&wait_startup_lock);
  return 0;
failed:
  if(sockfd != -1) {
    close(sockfd);
    sockfd = -1;
  }
  return -1;
}

void wait_for_listen_service_exit()
{
  if(listen_service_thread_id != 0){
    pthread_join(listen_service_thread_id, NULL);
    listen_service_thread_id = 0;
  }
}
