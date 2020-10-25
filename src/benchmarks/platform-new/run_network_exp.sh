#!/bin/sh

set -x

NUM_REPS=100
ROLE=${1:-master} # other is client
TYPE=${2:-udp} # values are mcast, udp, tcp
TAG=${3:-`date +'%Y%m%d'`}

SERVER=${4:-node-1}
PORT=${5:-6363}
BIND_ADDR=${6}

REQUEST_SIZES_NO_MACS='8 16 32 40 50 64 80 88 100 120 128 140 160 200'
REQUEST_SIZES="$REQUEST_SIZES_NO_MACS 400 800 1000 2000 3000 4000 6000 8000"

if [ "x${ROLE}" = "xmaster" ]; then
	for REP in `seq 1 $NUM_REPS`; do
		for REQS in $REQUEST_SIZES; do
			#echo $REQS $NUM_ITER
			./rt_master_mcast $TYPE $SERVER $PORT $REQS $NUM_ITER $BIND_ADDR 2>/dev/null|tee -a network-${TYPE}-${TAG}.txt;
  	done
	done
else
	./rt_client_mcast $TYPE $SERVER ${BIND_ADDR}
fi
