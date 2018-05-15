#ifndef __NETSVR_TIMER_H__
#define __NETSVR_TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif

int start_timer_service(void);
void wait_for_timer_service_exit();

struct Timer_Node;
typedef void (*timeout) (void);
/*创建定时器节点*/
struct Timer_Node *create_timer(int32_t handle,timeout cb);
/*开启定时器*/
void start_timer(struct Timer_Node *node,int32_t loop,int32_t sec);
/*停止定时器*/
void stop_timer(struct Timer_Node *node);
/*释放定时器节点*/
void close_timer(struct Timer_Node *node);

#ifdef __cplusplus
}
#endif

#endif
