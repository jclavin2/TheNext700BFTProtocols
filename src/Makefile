include Makefile.common

SUBDIRS = common
SUBDIRS += modular_bft
SUBDIRS += protocols/common
SUBDIRS += protocols/quorum
SUBDIRS += protocols/zlight
SUBDIRS += protocols/chain
SUBDIRS += protocols/pbft
SUBDIRS += protocols/pbft/replica
SUBDIRS += protocols/pbft/client

all: $(BFTLIB)
	$(MAKE) $(PARALLEL_MAKE) -C benchmarks ${MAKE_OPT}

$(BFTLIB): subdirs
	$(AR) rcv $@ `find $(SUBDIRS) -name *.o` 
	$(RANLIB) $@

subdirs:
	@for i in $(SUBDIRS); do \
		echo "'make all' in $$i..."; \
		${MAKE} ${PARALLEL_MAKE} -C $$i all; \
		echo "'make all' done in $$i"; \
	done

.PHONY: subdirs

clean: common-clean
	@for i in $(SUBDIRS); do \
		echo "make clean in $$i..."; \
		${MAKE} ${PARALLEL_MAKE} -C $$i clean; \
	done
	@${MAKE} ${PARALLEL_MAKE} -C benchmarks clean
	@-rm -f $(BFTLIB)

dep:
	@for i in $(SUBDIRS); do \
		echo "make depend in $$i..."; \
		${MAKE} ${PARALLEL_MAKE} -C $$i depend; \
	done

include Makefile.common.tail
