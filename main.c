#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "netsvr.h"
#include "netsvr_log.h"
#include "netsvr_client.h"
#include "netsvr_listen.h"
#include "message_queue.h"
#include "netsvr_worker.h"
#include "netsvr_timer.h"

int main(int argc, char *argv[])
{
//  uint16_t test;
//  char buf[] = {0x3C,0x5A,0xAA,0xAA};
//  memcpy(&test,buf,2);
//  printf("text == 0x%x \n",test);
  char puf[1024];
  strcpy(puf, "kkasfjkljdlkflkajdfjdhjhueoiruoiruew");
  printf("%s\n",puf);

  netsvr_mq_init();
  netsvr_set_log_level(NETSVR_INFO);

  if(start_worker_service(8) < 0){
    netsvr_logout(NETSVR_ERR,"start worker service failed !\n");
    return -1;
  }

  if(start_client_service(1) < 0){
    netsvr_logout(NETSVR_ERR,"start client service failed !\n");
    return -1;
  }

  if(start_listen_service() < 0){
    netsvr_logout(NETSVR_ERR,"start listen service failed !\n");
    return -1;
  }

  if(start_timer_service() < 0){
    netsvr_logout(NETSVR_ERR,"start timer service failed !\n");
    return -1;
  }
  
  netsvr_logout(NETSVR_INFO,"waiting timer service stop !\n");
  wait_for_timer_service_exit();
  netsvr_logout(NETSVR_INFO,"listen timer stop !\n");

  netsvr_logout(NETSVR_INFO,"waiting listen service stop !\n");
  wait_for_listen_service_exit();
  netsvr_logout(NETSVR_INFO,"listen service stop !\n");

  netsvr_logout(NETSVR_INFO,"waiting client service stop !\n");
  wait_for_client_service_exit();
  netsvr_logout(NETSVR_INFO,"listen client stop !\n");

  netsvr_logout(NETSVR_INFO,"waiting worker service stop !\n");
  wait_for_worker_service_exit();
  netsvr_logout(NETSVR_INFO,"listen worker stop !\n");
//  memcpy(puf,puf+20,strlen(puf)-20+1);
//  printf("%s\n",puf);

  //void *tmalloc = je_malloc(1024);
  return 0;
}

