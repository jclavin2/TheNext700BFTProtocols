include ../Makefile.common

all: objs

CXXFLAGS += $(PROTO_INCS)
CFLAGS   += $(PROTO_INCS)

libmodular_BFT.o: libmodular_BFT_choice.h

C_FILES=\
libmodular_BFT.cc

c_FILES= 

H_FILES := $(C_FILES:%.C=%.H)

h_FILES := $(c_FILES:%.c=%.h)

OBJ_FILES:=$(C_FILES:%.cc=%.o) $(c_FILES:%.c=%.o)

objs: $(OBJ_FILES)

clean: common-clean
clobber: common-clobber
	-rm -f libmodular_BFT.a

include ../Makefile.common.tail
