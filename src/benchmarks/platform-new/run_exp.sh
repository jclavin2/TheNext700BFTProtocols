#!/bin/sh

NUM_ITER=20000
NUM_REPS=100

REQUEST_SIZES_NO_MACS='8 16 32 40 50 64 80 88 100 120 128 140 160 200'
REQUEST_SIZES="$REQUEST_SIZES_NO_MACS 400 800 1000 2000 3000 4000 6000 8000"

for REP in `seq 1 $NUM_REPS`; do
	for REQS in $REQUEST_SIZES_NO_MACS; do
		#echo $REQS $NUM_ITER
		./auth_cost --no_macs $REQS $NUM_ITER 2>/dev/null|tee -a without_macs.txt;
  done
done

for REP in `seq 1 $NUM_REPS`; do
	for REQS in $REQUEST_SIZES; do
		#echo $REQS $NUM_ITER
		./auth_cost $REQS $NUM_ITER 2>/dev/null|tee -a with_macs.txt;
  done
done
