#ifndef __NETSVR_SERVER_H__
#define __NETSVR_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif
/*启动客户端连接管理池，thread_num为管理池的线程*/
int start_server_service(int thread_num);
void wait_for_server_service_exit();
/*将连接添加到客户端服务管理池*/
int add_server_to_server_service_pool(int clientfd, char *ip, int port);

/*客户端数据处理回调函数,提供给工作线程调用*/

#ifdef __cplusplus
}
#endif

#endif
