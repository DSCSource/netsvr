#ifndef __RW_SOCKET_H__
#define __RW_SOCKET_H__

#ifdef __cplusplus
extern "C" {
#endif

enum {
        CONNECT_ERR=-1,
        OUT_OF_MEM=-2,
        INVAILID_PARAM=-3,
};

int write_data(int sock,void*buf,int size);
int read_data(int sock,void*buf,int total);
int read_pack_data(int sock,void*buf,int max_size); // -1 socket error

#ifdef __cplusplus
}
#endif

#endif
