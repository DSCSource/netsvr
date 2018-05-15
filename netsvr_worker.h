#ifndef __NETSVR_WORKER_H__
#define __NETSVR_WORKER_H__

#ifdef __cplusplus
extern "C" {
#endif

void wakeup();
int exist_worker_exex();
int start_worker_service(int pthread);
void wait_for_worker_service_exit();

#ifdef __cplusplus
}
#endif

#endif

