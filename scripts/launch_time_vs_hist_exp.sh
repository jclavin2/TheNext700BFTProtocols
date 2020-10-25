#!/bin/bash

#NUMBER_OF_CLIENTS='1 2 4 6 8 10 12 14 18 20 40 80 100 150'
NUMBER_OF_FAULTS='1'

REQUEST_SIZES='8'
#REQUEST_SIZES='128 256 512 1024'
#REQUEST_SIZES='2048 4096'
REPLY_SIZES='8'
TIMES='0'
INIT_HISTORY_SIZES='0 50 100 150 200 250'

PROTO=${1:-ring}

NUM_RETRIES=3
TIMEOUT_VALUE=1600

OPT_FILE=ring_options.conf

./kill.sh

NC=1
for NB_F in $NUMBER_OF_FAULTS; do
perl -i -ple "s/NB_FAULTS=\d+/NB_FAULTS=$NB_F/" $OPT_FILE
for TIMS in $TIMES; do
	rm -rf /tmp/manager.out-*
	for REPS in $REPLY_SIZES; do
		for REQS in $REQUEST_SIZES; do
			for IHSIZE in $INIT_HISTORY_SIZES; do
				if [ x"$2" == x ]; then
        	OUTNAME=${PROTO}-${TIMS}s-${NB_F}.dat
      	else
        	OUTNAME=${PROTO}-${TIMS}s-${NB_F}-$2.dat
      	fi

				perl -i -ple "s/REQUEST_SIZE=\d+\s/REQUEST_SIZE=$REQS /" $OPT_FILE
				perl -i -ple "s/REPLY_SIZE=\d+\s/REPLY_SIZE=$REPS /" $OPT_FILE
				perl -i -ple "s/SLEEP_TIME=\d+\s/SLEEP_TIME=$TIMS /" $OPT_FILE
				perl -i -ple "s/INIT_HISTORY_SIZE=\d+\s/INIT_HISTORY_SIZE=$IHSIZE /" $OPT_FILE

				EXP_TRIES=0
				while [ $EXP_TRIES -lt $NUM_RETRIES ]; do
					TMOUT_VAL=$TIMEOUT_VALUE

					~/bin/doalarm ${TMOUT_VAL} ./launch_ring.sh $NC
					#~/bin/doalarm ${TMOUT_VAL} ./launch_redis.sh $NC
					if [ $? -ne 0 ]; then
						echo -n ''
						./kill.sh
						echo "Restarting loop $TIMS $REPS $REQS at $NC"
						#rm /tmp/manager.out-${NC}-${REQS}-${REPS}-${POM}
						rm /tmp/manager.out-${NC}-${REQS}-${REPS}
						let "EXP_TRIES+=1"
						sleep 120
					else
						echo -n ${IHSIZE} ${REQS} ${REPS} >> ~/times/switching.time
						grep 'response time'  /tmp/manager.out-${NC}-${REQS}-${REPS} >> ~/times/switching.time

						sleep 45
						break;
					fi
				done

				if [ $EXP_TRIES -ge $NUM_RETRIES ]; then
					echo "Exiting loop $TIMS $REPS $REQS at $NC"
					sleep 12
					break;
				fi
				sleep 60
			done
		done
	done

	sleep 10
done
done
