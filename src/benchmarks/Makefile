include ../Makefile.common

APPS=thr_client thr_manager replica_main multicast_replica

all: $(APPS)

INCLUDES += $(PROTO_INCS)

# DBT2 related stuff
#DBT2_INCS:=-Idbt2/src/include
#DBT2_LIBS:=-lm -lsqlite3 -lpthread
#DBT2_SOURCE:= dbt2/src/sqlite/dbc_common.c dbt2/src/nonsp/dbc_new_order.c \
                dbt2/src/nonsp/dbc_payment.c dbt2/src/nonsp/dbc_order_status.c \
                dbt2/src/nonsp/dbc_delivery.c dbt2/src/nonsp/dbc_stock_level.c \
                dbt2/src/nonsp/dbc_integrity.c dbt2/src/nonsp/dbc_nonsp.c \
		dbt2/src/common.c dbt2/src/logging.c dbt2/src/_socket.c \
        	dbt2/src/client_interface.c dbt2/src/transaction_data.c dbt2/src/_semaphore.c \
		dbt2/src/db.c \
		dbt2/src/input_data_generator.c

#DBT2_OBJS=$(DBT2_SOURCE:%.c=%.o)

# Redis related stuff
REDIS_INCS:=-I../redis/src/
REDIS_SOURCE:= \
../redis/src/adlist.c      ../redis/src/ae.c          \
../redis/src/anet.c \
../redis/src/sds.c \
../redis/src/zmalloc.c

REDIS_OBJS:=$(REDIS_SOURCE:%.c=%.o)

INCLUDES += $(DBT2_INCS) $(REDIS_INCS)
LIBS     += $(DBT2_LIBS) $(REDIS_LIBS)

DEFINES += -DLIBSQLITE -D_POSIX_C_SOURCE=199309

CFLAGS += --std=c99

# THis doesn't work for chain
#LIBS:=lib.a -lsfscrypt -lasync -lgmp -lpthread -ltcmalloc
LIBS:=$(BFTLIB) -lsfscrypt -lasync -lgmp -lpthread -lm -lrt #-ltcmalloc

C_FILES=\
thr_manager.cc \
incremental_stats.cc \
replica_main.cc \
replica_main_db.cc \
replica_main_redis.cc \
replica_main_ldap.cc \
thr_client.cc \
thr_client_db.cc  \
thr_client_redis.cc \
thr_client_ldap.cc

c_FILES=\
multicast_replica.c \
ddos_client.c ddos_server.c
# ldap_search.c

H_FILES := $(C_FILES:%.C=%.H)

h_FILES := $(c_FILES:%.c=%.h)

OBJ_FILES:=$(C_FILES:%.cc=%.o) $(c_FILES:%.c=%.o)


objs: $(DBT2_OBJS) $(OBJ_FILES)

$(APPS): $(BFTLIB) incremental_stats.o

# Redis
redis_objs: $(REDIS_OBJS)

%_redis: %_redis.cc redis_objs
	$(CCLINK) -o $@ $(CXXFLAGS) $(LIBDIRS) $< $(REDIS_OBJS) incremental_stats.o $(LIBS)

$(REDIS_OBJS): $(REDIS_SOURCE) ../redis/Makefile
	cd ../redis/src && make CC="${_C}" C="${_C}"

# DBT2

#dbt2_objs: $(DBT2_OBJS)

#%_db: %_db.cc dbt2_objs
	$(CCLINK) -o $@ $(CXXFLAGS) $(LIBDIRS) $< $(DBT2_OBJS) incremental_stats.o $(LIBS) -lsqlite3

# LDAP 

# LDAP_OBJS := ldap_search.o
# ldap_objs: $(LDAP_OBJS)

# %_ldap: %_ldap.cc ldap_objs
#	$(CCLINK) -o $@ $(CXXFLAGS) $(LIBDIRS) $< $(LDAP_OBJS) incremental_stats.o $(LIBS) -lldap -llber

# The rest
%: %.cc
	$(CCLINK) -o $@ $(CXXFLAGS) $(LIBDIRS) $< incremental_stats.o $(LIBS)

%: %.c
	$(CLINK) -o $@ $(CFLAGS) $(LIBDIRS) $< incremental_stats.o $(LIBS)

redis-benchmark: redis-benchmark.o redis_objs
	$(CLINK) -o $@ $(CFLAGS) $(LIBDIRS) $(REDIS_OBJS) redis-benchmark.o $(LIBS)


clean: common-clean
	-rm -f $(DBT2_OBJS)
	-rm -f $(APPS)
	-rm -f *~
	-rm -f *.o
	-cd ../redis/src && make clean

clobber: common-clobber
	-rm -f thr_manager thr_client server server_db

include ../Makefile.common.tail
