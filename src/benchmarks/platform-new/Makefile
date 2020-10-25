APPS=rt_master rt_client rt_master_mcast rt_client_mcast auth_cost

all: $(APPS)

CC:=ccache g++
C:=ccache gcc
AR:=ar
RANLIB:=ranlib
MAKEDEPEND:=makedepend 
INCS:=-I../../common -I../../common_bft -I../../abstract -I../../modular_bft \
	-I../../protocols/common -I../../protocols/quorum -I../../protocols/chain -I../../protocols/ring \
	-I../../../gmp -I../../../sfs/include/sfslite

#-I/usr/include/linux -I/usr/lib/gcc/i486-linux-gnu/4.3/include/ 
#CPPFLAGS:= $(INCS) -g -ggdb -Wall -DLIBSQLITE  
LIBDIRS:=-L../../gmp -L../../../sfs/lib/sfslite -L/usr/lib32
LIBS:=-lsfscrypt -lasync -lgmp -lpthread -lm -lsqlite3 -lldap -llber

CPPFLAGS:= $(INCS) -g -O0 -ggdb -m32 -Wall -DLIBSQLITE $(LIBDIRS) $(LIBS)

CFLAGS:= $(CPPFLAGS) -D_POSIX_C_SOURCE=199309
CFLAGS_SAFE:= $(CPPFLAGS) -g

%.o:: %.cc
	$(CC) $(CPPFLAGS) -o $@ -c $<

%.o:: %.c
	$(C) $(CFLAGS) -o $@ -c $<

auth_cost: auth_cost.o
	$(CC) -o $@ $(CPPFLAGS) $(LIBDIRS) $< $(LIBS) ../../protocols/common/Digest.o ../../common/umac.o

clean:
	-rm -f *.o
	-rm -f *~
	-rm $(APPS)

