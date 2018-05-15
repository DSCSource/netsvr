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
  
#include <signal.h> /*添加信号处理  防止向已断开的连接通信  */
  
/** 
 *   信号处理顺序说明：在Linux操作系统中某些状况发生时，系统会向相关进程发送信号， 
 *     信号处理方式是：1，系统首先调用用户在进程中注册的函数，2，然后调用系统的默认 
 *       响应方式,此处我们可以注册自己的信号处理函数，在连接断开时执行 
 *         */  
  
  
#define PORT    6331
#define Buflen  1024  
static const char *CONNECTED_CLIENT="NetClient";    //

void process_conn_client(int s,char *uid);  
//void sig_pipe(int signo);    /*用户注册的信号函数,接收的是信号值  */
  
int s;  /*全局变量 ， 存储套接字描述符  */
  
int main(int argc,char *argv[])  
{  
 
    if(argc<2) {
      printf("please input user id, the best use phone number!\n");
      return -1;
    } 
    struct sockaddr_in server_addr;  
    int err;  
    char server_ip[50] = "";  
    /********************socket()*********************/  
    s= socket(AF_INET,SOCK_STREAM,0);  
    if(s<0)  
    {  
        printf("client : create socket error\n");  
        return 1;  
    }  

    /*信号处理函数  SIGINT 是当用户按一个 Ctrl-C 建时发送的信号  */
  
    /*******************connect()*********************/  
    /*设置服务器地址结构，准备连接到服务器  */
    memset(&server_addr,0,sizeof(server_addr));  
    server_addr.sin_family = AF_INET;  
    server_addr.sin_port = htons(PORT);  
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  
  
    /*将用户数入对额字符串类型的IP格式转化为整型数据*/  
    /*inet_pton(AF_INET,argv[1],&server_addr.sin_addr.s_addr);  */
    //printf("please input server ip address : \n");  
    //read(0,server_ip,50);  
    /*err = inet_pton(AF_INET,server_ip,&server_addr.sin_addr.s_addr);  */
    //server_addr.sin_addr.s_addr = inet_addr(server_ip);  
  
    err = connect(s,(struct sockaddr *)&server_addr,sizeof(struct sockaddr));  
    if(err == 0)  
    {  
        printf("client : connect to server\n");  
    }  
    else  
    {  
        printf("client : connect error\n");  
        return -1;  
    }  
    /*与服务器端进行通信  */
    process_conn_client(s, argv[1]);  
    close(s);  
  
}

int write_data(int sock,void*buf,int size)
{
  int now = 0;
  int sended=0;
  int retry=0;
  if(sock==-1||sock==0||buf==NULL||size==0) {
    return 0;
  }
  while(sended<size) {
    now=write(sock,buf+sended,size-sended);
    if(now<0) {
      if(now == EINTR || now == EWOULDBLOCK){
        continue;
      }else if(now==EAGAIN)  /* EAGAIN : Resource temporarily unavailable*/
      {
        usleep(1000);   /*等待防止cpu %100，希望发送缓冲区能得到释放*/
        continue;
      }else
      {
        return -1;
      }
    }
    else if(now==0) {
      if(retry<5) {
        retry++;
        continue;
      }
      else {
        return -1;
      }
    }
    else {
      sended+=now;
    }
  }
  return size;
}
  
void process_conn_client(int s, char *uid)  
{  
  
    int data_size = 0;  
    int uid_size = 0;
    char buffer[Buflen];  
    char buf[8]={0};
    printf("int size =%d \n",sizeof(int));
    memset(buffer, '\0', Buflen);
    data_size = strlen(CONNECTED_CLIENT)+1;
    memcpy(buffer,&data_size,sizeof(int));
    memcpy(buffer+sizeof(int),CONNECTED_CLIENT, data_size);
    uid_size = strlen(uid)+1;
    memcpy(buffer+sizeof(int)+data_size,&uid_size,sizeof(int));
    memcpy(buffer+2*sizeof(int)+data_size,uid, uid_size);
    data_size = 22+uid_size;
    memcpy(buf,&data_size,sizeof(int));
    int len=write_data(s, &data_size, sizeof(int));
    if(len!=4) {
      printf("write socket error when in client_cerf send data len %d!!!\n",4);
      return ;
    }
    printf("write data size :%d data:%d\n",len,data_size);
    len=write_data(s, buffer, data_size-4);
    if(len!=(data_size-4)) {
     printf("write socket error when send data length:%d!!!\n",data_size-4);
     return;
    }  
    printf("write data size :%d \n",len);
    for(;;)  
    {  
        memset(buffer,'\0',Buflen);  
        /*从标准输入中读取数据放到缓冲区buffer中*/  
        data_size = read(0,buffer,Buflen);   /* 0，被默认的分配到标准输入  1，标准输出  2，error  */
        if(data_size >  0)  
        {  
            /*当向服务器发送 “quit” 命令时，服务器首先断开连接  */
            data_size = strlen(buffer)+5;
            write(s, &data_size, sizeof(int));   /*向服务器端写  */
            write(s, buffer, strlen(buffer)+1);   /*向服务器端写  */
  
            /*等待读取到数据  */
            //for(data_size = 0 ; data_size == 0 ; data_size = read(s,buffer,Buflen) );  
  
            //write(1,buffer,strlen(buffer)+1);   /*向标准输出写  */
        }  
    }  
}  
#if 0 
void sig_pipe(int signo)    /*传入套接字描述符  */
{  
    printf("Catch a signal\n");  
    if(signo == SIGTSTP)  
    {  
  
        printf("接收到 SIGTSTP 信号\n");  
        int ret = close(s);  
        if(ret == 0)  
            printf("成功 : 关闭套接字\n");  
        else if(ret ==-1 )  
            printf("失败 : 未关闭套接字\n");  
        exit(1);  
    }  
}
#endif
