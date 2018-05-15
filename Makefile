################################################################################
#
################################################################################
.slient:
ifdef COMSPEC
APP_TOP	:= $(shell cd)
else
APP_TOP := $(shell pwd)
endif

#NEXUS_TOP = $(APP_TOP)/../nexus

#include tools.inc
#include flag.inc

AS      = as
LD      = ld
CC      = gcc
#CC      = gcc
CXX     = g++
#CXX     = c++
AR      = ar
NM      = nm
STRIP   = strip
OBJCOPY = objcopy
OBJDUMP = objdump
RANLIB  = ranlib
READELF = readelf
MKDIR   = mkdir -p
PWD     = pwd
MV      = mv
PERL    = perl

CPP     = $(CC) -E
CP      = cp -fp
RM      = rm -f
SORT    = sort
SED     = sed
TOUCH   = touch
AWK     = awk

#Q_ = @


#APP_INC_PATH = $(APP_TOP)/granddog/include 
#APP_LIB_PATH =$(APP_TOP)/granddog/lib

DEFINES	+= \

CFLAGS	+= \
	   $(NEXUS_CFLAGS) \
	   -I$(APP_TOP) \
           -I$(APP_TOP)/include/ \
	   $(DEFINES)


CXXFLAGS	+= \
		   $(CLAGS)


LDFLAGS         += \
#		-Wl,-rpath-link,/imono/lib

VPATH	+= \
	   $(APP_TOP)

vpath %.c $(VPATH)
vpath %.cpp $(VPATH)
#./lib/libjemalloc_pic.a
LIBS = -L./lib ./lib/libjemalloc_pic.a -lpthread -lm -lrt -ldl
TARGET	= netsvr

APP_OBJS	+= \
                    main.o \
                    rw_socket.o \
                    message_queue.o \
                    malloc_hook.o \
                    netsvr_listen.o \
                    netsvr_server.o \
                    netsvr_log.o \
                    netsvr_client.o \
                    netsvr_hash.o \
                    netsvr_worker.o \
                    netsvr_timer.o \
                    cJSON/cJSON.o
#                   netsvr_timer.o \
#                   netsvr_log.o \
#                   malloc_hook.o 
#                   main.o
#                   tty_select.o \
#                   tty_heart.o \
#                   tty_init.o 
#		   main.o # ./granddog/imonodog.o


.PHONY: all clean


all: $(TARGET)

$(TARGET): $(APP_OBJS) 
	$(CC) -o $(APP_TOP)/$@ $(addprefix $(APP_TOP)/, $(APP_OBJS)) $(LDFLAGS) $(LIBS)
	rm *.o  cJSON/*.o

clean:
	$(RM) -r *.o *~ cJSON/*.o



	
debug:
	@echo APP_TOP = $(APP_TOP)
	@echo APP_SRC_PATH = $(APP_TOP)
	@echo APP_LIB_PATH = $(APP_LIB_PATH)
	@echo APP_LIBS = $(APP_LIBS)
	@echo APP_OBJS = $(APP_OBJS)



