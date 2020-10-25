#!/usr/bin/zsh

#RATES='250 500 600 700 750 800 850 900 950 1000 1050 1100 1200 1300 1400 1500'
RATES=( 90 95 100 105 110 ) # in Mbps
#RATES=( 87.5 92.5 97.5  ) # in Mbps
#RATES='85 95 105' # in Mbps

REPETITIONS=20
NUM_R=( 10 7 4 )

for NC in $NUM_R; do
for MRATE in $RATES; do
	RATE=$(( MRATE * 1000000 / NC ))
	echo Rate is $RATE
	for TRY in `seq 1 $REPETITIONS`; do
		echo REPETITION $TRY
		#perl -MList::Util -e '$n=shift;$n--;@l=<>;@o=List::Util::shuffle(@l);print @o[0..$n];' $NC mcast.hosts >mcast.hosts.pssh
		perl -MList::Util -e '$n=shift;$n--;@l=<>;print @l[0..$n];' $NC mcast.hosts >mcast.hosts.pssh
		echo Will run "~/iperf.client.sh $RATE 120"
		parallel-ssh -v -t 180 --print -h mcast.hosts.pssh "~/iperf.client.sh $RATE 120" </dev/null |tee -a ~/times/iperf.clients.out
		sleep 5;
	done
	sleep 20
done
done