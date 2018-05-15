#ifndef __NETSVR_LISTEN_H__
#define __NETSVR_LISTEN_H__

#ifdef __cplusplus
extern "C" {
#endif

int start_listen_service();
void wait_for_listen_service_exit();

#ifdef __cplusplus
}
#endif

#endif
