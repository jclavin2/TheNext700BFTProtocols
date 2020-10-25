#!/bin/zsh

EXP_FILE=./launch_x.sh
REPS=40
TAG=${1:-`date +'%y%m%d'`}

#echo "#define RUN_QUORUM" > ~/BFT/src/modular_bft/libmodular_BFT_choice.h
#(cd ~/BFT/src && make all clean || make all || make all) || exit
#$EXP_FILE quorum

echo "#define RUN_CHAIN" > ~/BFT/src/modular_bft/libmodular_BFT_choice.h
(cd ~/BFT/src && make clean all || make all || make all) || exit
for i in {1..$REPS}; do
	$EXP_FILE chain $TAG-$i
done

echo "#define RUN_ZLIGHT" > ~/BFT/src/modular_bft/libmodular_BFT_choice.h
(cd ~/BFT/src && make clean all || make all || make all) || exit
for i in {1..$REPS}; do
	$EXP_FILE zlight $TAG-$i
done

echo "#define RUN_PBFT" > ~/BFT/src/modular_bft/libmodular_BFT_choice.h
(cd ~/BFT/src && make clean all || make all || make all) || exit
for i in {1..$REPS}; do
	$EXP_FILE pbft $TAG-$i
done

#echo "#define RUN_RING" > ~/BFT/src/modular_bft/libmodular_BFT_choice.h
#(cd ~/BFT/src && make clean all || make all || make all) || exit
#$EXP_FILE ring

