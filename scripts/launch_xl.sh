#!/bin/bash

#set -x

PROTO=${1:-ring}

# used only for file names
NB_F=1

NUM_RETRIES=3
TIMEOUT_VALUE=1800

EXPERIMENTS_FILE=exp_variants.conf
OPT_FILE=ring_options.conf

if [ x"$2" == x ]; then
  OUTNAME=${PROTO}-${NB_F}-variants.dat
else
  OUTNAME=${PROTO}-${NB_F}-variants-$2.dat
fi

if [ -e ~/times/${OUTNAME} ]; then
  echo ~/times/${OUTNAME} exists, quitting...
  continue;
fi

./kill.sh

exec 9<${EXPERIMENTS_FILE}
while read -u9 line; do
	echo In da loop....
	[[ $line = \#* ]] && continue
	if [[ ! $line =~ [^[:space:]] ]] ; then
		continue;
	fi

	TEXEC=$(echo $line | awk '{print $1}')
	REQS=$(echo $line | awk '{print $2}')
	REPS=$(echo $line | awk '{print $3}')
	NC=$(echo $line | awk '{print $4}')

	echo "Running $TEXEC $REQS $REPS $NC with $PROTO"

	rm -rf /tmp/manager.out-*
	DID_200=0
	#if [ -e ~/times/${PROTO}-${TEXEC}s-${NB_F}.dat ]; then
	#continue;
	#fi

	#if [ $REQS -ge 4000 -a $NC -ge 50 ]; then
	#break
	#fi
	#if [ "x$PROTO" = "xquorum" -a $NC -ge 8 ]; then
	#echo "WILL EXIT AT $NC $REQS $REPS $TEXEC"
	#./kill.sh
	#break;
	#fi
	#if [ "$DID_200" = "1" ]; then
	#break;
	#fi
	perl -i -ple "s/REQUEST_SIZE=\d+\s/REQUEST_SIZE=$REQS /" $OPT_FILE
	perl -i -ple "s/REPLY_SIZE=\d+\s/REPLY_SIZE=$REPS /" $OPT_FILE
	perl -i -ple "s/SLEEP_TIME=\d+\s/SLEEP_TIME=$TEXEC /" $OPT_FILE
	#perl -i -ple "s/PERCENT_MALICIOUS=\d+/PERCENT_MALICIOUS=$POM/" $OPT_FILE

	EXP_TRIES=0
	while [ $EXP_TRIES -lt $NUM_RETRIES ]; do
		TMOUT_VAL=$TIMEOUT_VALUE
		if [ $NC -le 140 ]; then
			TMOUT_VAL=1200
		fi

		PROTO=$PROTO ~/bin/doalarm ${TMOUT_VAL} ./launch_ring.sh $NC
		#~/bin/doalarm ${TMOUT_VAL} ./launch_redis.sh $NC
		if [ $? -ne 0 ]; then
			echo -n ''
			./kill.sh
			echo "Restarting loop $TEXEC $REPS $REQS at $NC"
			#rm /tmp/manager.out-${NC}-${REQS}-${REPS}-${POM}
			rm /tmp/manager.out-${NC}-${REQS}-${REPS}
			let "EXP_TRIES+=1"
			sleep 120
		else
			# if experiment succeeds
			echo "::${OUTNAME} [$NC] [$REQS] [$REPS]" >> ~/times/timings.stat
      grep SAMPLES /tmp/server*.out >> ~/times/timings.stat
      grep TIMINGS /tmp/server*.out >> ~/times/timings.stat

			echo '>>>>' $TEXEC $REQS $REPS $NC >> ~/times/variants.out
			cat /tmp/manager.out-${NC}-${REQS}-${REPS} >> ~/times/variants.out
			sleep 45
			break
		fi
	done

	echo "HEEEEREEEEEEEEEEEE"
	if [ $EXP_TRIES -ge $NUM_RETRIES ]; then
		echo "Exiting loop $TEXEC $REPS $REQS at $NC"
		sleep 15
		continue
		#if [ $NC = "200" ]; then
		#continue;
		#else
		#break;
		#fi
	fi
	echo "Will continue"
	continue

done

set +x
