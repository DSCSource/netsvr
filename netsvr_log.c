#include <stdio.h>
#include <stdarg.h> 
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#include "netsvr.h"
#include "./netsvr_log.h"

#define MAXBUF 16
#define DEFAULT_PRECI 3
#define LOG_TIME_SIZE 32
#define LOG_MESSAGE_SIZE 256

static int log_level=1;
static char log_time[LOG_TIME_SIZE];
static char log_tmp[LOG_MESSAGE_SIZE];
void netsvr_set_log_level(int level)
{
  if(level<NETSVR_ERR||level>NETSVR_INFO) {
    return;
  }
  log_level=level;
}

int netsvr_logout(int type,const char*format,...)
{
  if(type>log_level) {
    return 0;
  }
  //printf("%s",head);
  char *data = NULL;
  va_list ap;

  va_start(ap,format);
  int len = vsnprintf(log_tmp, LOG_MESSAGE_SIZE, format, ap);
  va_end(ap);
  
  if (len >=0 && len < LOG_MESSAGE_SIZE) {
    data = netsvr_strdup(log_tmp);
  } else {
    int max_size = LOG_MESSAGE_SIZE;
    for (;;) {
      max_size *= 2;
      data = netsvr_malloc(max_size);
      va_start(ap,format);
      len = vsnprintf(data, max_size, format, ap);
      va_end(ap);
      if (len < max_size) {
        break;
      }
      netsvr_free(data);
    }
  }
  if (len < 0) {
    netsvr_free(data);
    perror("vsnprintf error :");
    return;
  }
  time_t time_log = time(NULL);
  struct tm* tm_log = localtime(&time_log);
  sprintf(log_time,"[%04d-%02d-%02d %02d:%02d:%02d]",tm_log->tm_year+1900,tm_log->tm_mon+1,tm_log->tm_mday,tm_log->tm_hour,tm_log->tm_min,tm_log->tm_sec);
  switch(type) {
  case NETSVR_WARN:
    printf("%s [Warning]:%s",log_time,data);
    break;
  case NETSVR_ERR:
    printf("%s [Error]:%s",log_time,data);
    break;
  case NETSVR_IMPORT:
    printf("%s [Import]:%s",log_time,data);
    break;
  case NETSVR_INFO:
    printf("%s [Info]:%s",log_time,data);
    break;
  }
  netsvr_free(data);
#if 0
  struct netsvr_message smsg;
  if (context == NULL) {
    smsg.source = 0;
  } else {
    smsg.source = skynet_context_handle(context);
  }
  smsg.session = 0;
  smsg.data = data;
  smsg.sz = len | ((size_t)PTYPE_TEXT << MESSAGE_TYPE_SHIFT);
  skynet_context_push(logger, &smsg);
#endif
  return 0; 
} 





