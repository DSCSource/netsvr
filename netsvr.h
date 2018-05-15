#ifndef __NETSVR_H__
#define __NETSVR_H__

#include "netsvr_malloc.h"
#include "netsvr_log.h"
#include <stddef.h>
#include <stdint.h>

#define PTYPE_TEXT 0
#define PTYPE_RESPONSE 1
#define PTYPE_MULTICAST 2
#define PTYPE_CLIENT 3
#define PTYPE_SYSTEM 4
#define PTYPE_HARBOR 5
#define PTYPE_SOCKET 6
// read lualib/skynet.lua examples/simplemonitor.lua
#define PTYPE_ERROR 7
// read lualib/skynet.lua lualib/mqueue.lua lualib/snax.lua
#define PTYPE_RESERVED_QUEUE 8
#define PTYPE_RESERVED_DEBUG 9
#define PTYPE_RESERVED_LUA 10
#define PTYPE_RESERVED_SNAX 11
//
#define PTYPE_TAG_DONTCOPY 0x10000
#define PTYPE_TAG_ALLOCSESSION 0x20000

#define cast(t, exp)    ((t)(exp))

#define cast_byte(i)    cast(unsigned char, (i))
#define cast_void(i)    cast(void, (i))
#define cast_int(i)     cast(int, (i))
#define cast_uchar(i)   cast(unsigned char, (i))

//
enum NETSVR_LOG_TYPE{
  NETSVR_ERR=0,   /*错误日志（写入到错误日志文件）*/
  NETSVR_WARN=1,  /*警告日志*/
  NETSVR_IMPORT=2,/*重要日志（写入到重要日志文件）*/
  NETSVR_INFO=3,  /*打印日志*/
};

struct skynet_context;

//void netsvr_logout(int type, const char *format, ...);

#endif
