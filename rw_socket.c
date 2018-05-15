#include <stdio.h>
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
#include <pthread.h>
#include <netinet/tcp.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <time.h>

#include "rw_socket.h"

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

int read_data(int sock,void*buf,int total)
{
  int now = 0;
  int readed=0;
  int retry=0;

  if(total<=0||buf==NULL) {
    return INVAILID_PARAM;
  }

  while(readed<total) {
    now=read(sock, buf+readed, total-readed);
    if(now<0) {
      if(now == EINTR || now == EWOULDBLOCK){
        continue;
      }else if(now==EAGAIN)  /* EAGAIN : Resource temporarily unavailable*/
      {
        usleep(1000);   /*等待防止cpu %100，希望发送缓冲区能得到释放*/
        continue;
      }else
      {
        return CONNECT_ERR;
      }
    }
    else if(now==0) {
      if(retry<5) {
        retry++;
        continue;
      }
      else {
        return CONNECT_ERR;
      }
    }
    else {
      readed+=now;
    }
  }
  return readed;
}

int read_pack_data(int socket,void*buf,int max_size)
{
  int nLen = 0;
  int ret=OUT_OF_MEM;

  if((ret = read_data(socket,&nLen,sizeof(int))) < 0) {
    return CONNECT_ERR;
  }else if(ret == 0)
  {
    return 0;
  }
  //printf("pack size=%d\n",nLen);
  nLen-=4;
  if(nLen>max_size) {
    read(socket, buf, max_size);
    return OUT_OF_MEM;
  }
  else {
    ret=nLen;
  }
  if(read_data(socket,buf,nLen)<0) {
    return CONNECT_ERR;
  }
  return ret;
}
