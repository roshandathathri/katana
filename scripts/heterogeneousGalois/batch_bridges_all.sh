#!/bin/sh

#EXECS=( "bfs_pull-topological" "pagerank_pull-topological" "cc_pull-topological" "sssp_pull-topological" "bfs_push-filter" "pagerank_push-filter" "cc_push-filter" "sssp_push-filter" "bfs_push-worklist" "pagerank_push-worklist" "cc_push-worklist" "sssp_push-worklist" "bfs_push-topological" "pagerank_push-topological" "cc_push-topological" "sssp_push-topological" )
#EXECS=( "bfs_push-filter_comm-updated-only" "pagerank_push-filter_comm-updated-only" "cc_push-filter_comm-updated-only" "sssp_push-filter_comm-updated-only" )
EXECS=( "bfs_push-filter_comm-updated-only")

#SET="1,4:00:00 2,03:30:00 4,03:00:00 8,02:30:00 16,02:00:00 32,01:45:00 64,01:30:00 128,01:00:00"
SET="2,03:30:00 "

#INPUTS=("rmat20;\"${SET}\"")
#INPUTS=("uk-2007;\"${SET}\"")
#INPUTS=("twitter-ICWSM10-component;\"${SET}\"")
INPUTS=("rmat28;\"${SET}\"")
#INPUTS=("clueweb12;\"${SET}\"")

#QUEUE=GPU-shared
#QUEUE=GPU
QUEUE=RM
VCUT="cart_vcut"
current_dir=$(dirname "$0")
env_vcut=0

echo "current dir : $current_dir"
for j in "${INPUTS[@]}"
do
  IFS=";";
  set $j;
  for i in "${EXECS[@]}"
  do
      echo "ABELIAN_VERTEX_CUT=${env_vcut} $current_dir/run_bridges_all.sh ${i} ${1} ${2} $QUEUE $VCUT"

      ABELIAN_VERTEX_CUT=${env_vcut} $current_dir/run_bridges_all.sh ${i} ${1} ${2} $QUEUE $VCUT |& tee -a jobs
  done
done
