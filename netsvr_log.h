#ifndef __NETSVR_SERVICE_LOG_H__
#define __NETSVR_SERVICE_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

void netsvr_set_log_level(int level);
int netsvr_logout(int type,const char*format,...);


#ifdef __cplusplus
}
#endif


#endif
