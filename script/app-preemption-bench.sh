#!/usr/bin/env bash

script=$(basename "${BASH_SOURCE[0]}")
path=$(dirname $(readlink -f "${BASH_SOURCE[0]}"))
short="n:c:m:f:s:b:p:S:B:P:h"
long="nbnodes:nbcores:mempercores:small:big:preemptive:nbsmall:nbbig:nbpremptive:help"
opts=$(getopt -o $short --long $long --name "$script" -- "$@")
usage="\nTest preemption capability and dalays\n
Usage : ${script}\n
   -n : number of nodes in the cluster
   -c : number of cores per nodes
   -m : size of memory per mpi core (MB)
   -f : fraction of nodes for large and preemptive jobs (%)

   -s : small job configuration [optional]
   -b : big job configuration [optional]
   -p : premptive job configuration [optional]
   -S: number of small configuration jobs [optional]
   -B: number of big configurations jobs [optional]
   -P: number of premptive jobs [optional]
   example : ${script} -n 1500 -c 80 -m 20 -f 10\n"

eval set -- "${opts}"

while :; do
    case "${1}" in
        -n | --nbnodes      ) NB_NODES=$2;             shift 2 ;;
        -c | --nbcores      ) NB_CORES=$2;             shift 2 ;;
        -m | --mempercores  ) MEM=$2;                  shift 2 ;;
        -f | --pccores      ) PC_CORES=$2;             shift 2 ;;

        -s | --small        ) CONFIG_SMALL=$2;         shift 2 ;;
        -b | --big          ) CONFIG_BIG=$2;           shift 2 ;;
        -p | --preemptive   ) CONFIG_PREEMPTIVE=$2;    shift 2 ;;
        -S | --nbsmall      ) NB_SMALL=$2;             shift 2 ;;
        -B | --nbbig        ) NB_BIG=$2;               shift 2 ;;
        -P | --nbpreemptive ) NB_PREEMPTIVE=$2;        shift 2 ;;
        -h | --help         ) echo -e "${usage}" 1>&2; exit ;;
        --                  ) shift;                   break ;;
        *                   ) echo "error parsing";    exit 1 ;;
    esac
done

#----- BEGIN PROVIDER SPECIFIC DEFINITIONS
QSystem=PBS                                                  # Queuing system
Queue=development                                            # Regular queue
QueuePremptive=production                                    # Preemptive queue
Delay=5                                                      # Delay for all regular jobs to start running

# Define specific MPI environment
MPI_ENV="
. r.load.dot mrd/rpn/code-tools/latest/env/inteloneapi-2025.1.0

export PATH=${path}:\$PATH
"
#----- END PROVIDER SPECIFIC DEFINITIONS

NB_NODES=${NB_NODES:-4}                                      # Number of nodes on cluster
NB_CORES=${NB_CORES:-80}                                     # Number of cores per node
MEM=20                                                       # Memory per mpi core
PC_NODES=${PC_NODES:-10}                                     # % of cluster for big jobs

eval pcnodes=\`perl -e \'print int\(${NB_NODES}*${PC_NODES}/100.0+0.99\)\'\`

CONFIG_SMALL=${CONFIG_SMALL:-1}                              # Small jobs (1 node)
CONFIG_BIG=${CONFIG_BIG:-${pcnodes}};                        # Big jobs (% of cluster)
CONFIG_PREEMPTIVE=${CONFIG_PREEMPTIVE:-${pcnodes}}           # Preemptive jobs (10% of cluster)
NB_BIG=${NB_BIG:-$((NB_NODES/$CONFIG_BIG-2))};               # Number of big jobs
NB_SMALL=${NB_SMALL:-$((NB_NODES-($NB_BIG*$CONFIG_BIG)))};   # Number of small jobs
NB_PREEMPTIVE=${NB_PREEMPTIVE:-2}                            # Number of preemptive jobs

echo "(INFO) Submitting small=${NB_SMALL}x${CONFIG_SMALL} + big=${NB_BIG}x${CONFIG_BIG} + preemptive=${NB_PREEMPTIVE}x${CONFIG_PREEMPTIVE}"

prepjob() {

   local nbnode=${1}
   local id=${2}
   local sz=$((${nbnode} * ${NB_CORES}))

   # Qeueing system specific params
   case ${QSystem} in
      "PBS")
         command="qsub -q "
         cat <<EOT > job${id}.sh
#!/bin/bash
#PBS -l select=${nbnode}:ncpus=${sz}:mpiprocs=${sz}:ompthreads=1:mem=${MEM}G
#PBS -l walltime=0:30:0
EOT
         ;;
      "SLURM")
         command="sbatch --partition="
         cat <<EOT > job${id}.sh
#!/bin/bash
#SBATCH --job-name=job${id}
#SBATCH --ntasks=${nbnode}
#SBATCH --cpus-per-task=${sz}
#SBATCH --mem-per-cpu=${MEM}G
#SBATCH --time=0:30:0
#SBATCH --account=eccc_mrd 
EOT
         ;;

      "*") # PROVIDER SPECIFIC DEFINITIONS (Other scheduler)
         cat <<EOT > job${id}.sh
EOT
         ;;
   esac

   # Job per se
   cat <<EOT >> job${id}.sh

# Script creation time
secs0=$(date +%s)

# Define specific MPI environment
${MPI_ENV}

# Start MPI
mpirun -n ${sz} app -s 0 -t ${id} -q \${secs0}
EOT
}

jids=()

# Launch big jobs
echo "(INFO) Launching $NB_BIG big config (MPI=$((${CONFIG_BIG}*${NB_CORES})))"
prepjob ${CONFIG_BIG} Big

for n in $(seq $NB_BIG); do
   jid=`${command}${Queue} jobBig.sh` 
   jids+=(${jid})
done

# Launch small jobs
echo "(INFO) Launching $NB_SMALL small config (MPI=$((${CONFIG_SMALL}*${NB_CORES})))"
prepjob ${CONFIG_SMALL} Small

for n in $(seq $NB_SMALL); do
   jid=`${command}${Queue} jobSmall.sh` 
   jids+=(${jid})
done

# Queue 10 more
echo "(INFO) Queuing 10 more small config (MPI=$((${CONFIG_SMALL}*${NB_CORES})))"
for n in $(seq 10); do
   jid=`${command}${Queue} jobSmall.sh` 
   jids+=(${jid})
done

sleep ${Delay}

#----- PROVIDER SPECIFIC DEFINITIONS (Preemption method)
# Preempt jobs (SIGTERM method test)
echo "(INFO) Sending SIGTERM signal ${jids[@]}"
qsig -s SIGTERM ${jids[@]} 

# Launch preemptive jobs
echo "(INFO) Launching $NB_PREEMPTIVE preemptive config (MPI=$((${CONFIG_PREEMPTIVE}*${NB_CORES})))"
prepjob ${CONFIG_PREEMPTIVE} Preemptive

for n in $(seq $NB_PREEMPTIVE); do
   jid=`${command}${QueuePremptive} jobPreemptive.sh`
done