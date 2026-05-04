#!/usr/bin/env bash
#\rm *.sh.e* *.sh.o*; cp ../script/app-preemption-bench.sh .; cp ./src/utils/app .;./app-preemption-bench.sh -n 3 -c 128 -m 400 -b 1 -B 0 -S 3 -s 1 -p 1 -P 2 -i 30

script=$(basename "${BASH_SOURCE[0]}")
path=$(dirname $(readlink -f "${BASH_SOURCE[0]}"))
short="n:c:m:i:f:a:s:b:p:S:B:P:th"
long="nbnodes:nbcores:mempercores:nbiterations:fraction:trap:small:big:preemptive:nbsmall:nbbig:nbpremptive:test:help"
opts=$(getopt -o $short --long $long --name "$script" -- "$@")
usage="\nTest preemption capability and dalays\n
Usage : ${script}\n
   -n : number of nodes in the cluster
   -c : number of cores per nodes
   -m : size of memory per mpi core (MB)
   -i : Number of simulated iteration seconds (default: 100)
   -f : fraction of nodes for large and preemptive jobs (%)
   -t : test mode, does not launch the jobs
   -a : trap delay(s) before answering a signal (default:1, 0=no answering)

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
        -f | --pccores      ) PC_NODES=$2;             shift 2 ;;
        -i | --nbiterations ) NB_ITER=$2;              shift 2 ;;

        -s | --small        ) CONFIG_SMALL=$2;         shift 2 ;;
        -b | --big          ) CONFIG_BIG=$2;           shift 2 ;;
        -p | --preemptive   ) CONFIG_PREEMPTIVE=$2;    shift 2 ;;
        -S | --nbsmall      ) NB_SMALL=$2;             shift 2 ;;
        -B | --nbbig        ) NB_BIG=$2;               shift 2 ;;
        -t | --test         ) TEST=1;                  shift 1 ;;
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
Delay=10                                                     # Delay before launching preemptive jobs

# Define specific MPI environment
MPI_ENV="
. r.load.dot mrd/rpn/code-tools/latest/env/inteloneapi-2025.1.0

export PATH=${path}:\$PATH
"
#----- END PROVIDER SPECIFIC DEFINITIONS

NB_NODES=${NB_NODES:-4}                                      # Number of nodes on cluster
NB_CORES=${NB_CORES:-80}                                     # Number of cores per node
NB_ITER=${NB_ITER:-100}                                      # Number of iterations
MEM=${MEM:-20}                                               # Memory per mpi core
PC_NODES=${PC_NODES:-10}                                     # % of cluster for big jobs
TEST=${TEST:-0}                                              # Test mode
TRAP_DELAY=${TRAP_DELAY:-1}                                  # Delay before answering a signal

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
   local step=${3}
   local trapd=${4}
   local sz=$((${nbnode} * ${NB_CORES}))

   # Qeueing system specific params
   case ${QSystem} in
      "PBS")
         command="qsub -q "
         cat <<EOT > job${id}.sh
#!/bin/bash
#PBS -l select=${nbnode}:ncpus=${sz}:mpiprocs=${sz}:ompthreads=1:mem=${MEM}G
#PBS -l walltime=0:30:0

# Sequence number of job
seq=\${PBS_JOBID}
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

# Sequence number of job
seq=\${$SLURM_JOB_ID}
EOT
         ;;

      "*") # PROVIDER SPECIFIC DEFINITIONS (Other scheduler)
         cat <<EOT > job${id}.sh
EOT
         ;;
   esac

   # Job per se
   cat <<EOT >> job${id}.sh
cd $path

export APP_VERBOSE_TIME=SECOND

signal_mpi() {
   echo "Caught signal, signaling MPI process \$mpi_pid"
   kill -SIGUSR2 \$mpi_pid
}
#trap 'signal_mpi' SIGTERM SIGUSR2 SIGUSR1 SIGURG
trap '' SIGTERM SIGUSR2 SIGUSR1 SIGURG

# Script launch time
secs0=$(date +%s)

# Define specific MPI environment
${MPI_ENV}

# Start MPI
mpirun -n ${sz} app -t ${id}-\${seq} -q \${secs0} -s ${step} -d 1 -v INFO -a ${trapd} -l ${id}-\${seq}.\$\$.out
#mpi_pid=\$!
#wait \$mpi_pid
EOT
}

jids=()

# Launch big jobs
echo "(INFO) Launching $NB_BIG big config (MPI=$((${CONFIG_BIG}*${NB_CORES})))"
prepjob ${CONFIG_BIG} Big ${NB_ITER} ${TRAP_DELAY}
for n in $(seq $NB_BIG); do
   [[ ${TEST} -eq 0 ]] && jid=`${command}${Queue} jobBig.sh` 
   jids+=(${jid})
done

# Launch small jobs
echo "(INFO) Launching $NB_SMALL small config (MPI=$((${CONFIG_SMALL}*${NB_CORES})))"
prepjob ${CONFIG_SMALL} Small ${NB_ITER} ${TRAP_DELAY}

for n in $(seq $NB_SMALL); do
   [[ ${TEST} -eq 0 ]] && jid=`${command}${Queue} -r y jobSmall.sh` 
   jids+=(${jid})
done

# Queue 10 more
#echo "(INFO) Queuing 10 more small config (MPI=$((${CONFIG_SMALL}*${NB_CORES})))"
#for n in $(seq 10); do
#TODO [[ ${TEST} -eq 0 ]] && jid=`${command}${Queue} jobSmall.sh` 
#   jids+=(${jid})
#done

[[ ${TEST} -eq 0 ]] && sleep ${Delay}

#----- PROVIDER SPECIFIC DEFINITIONS (Preemption method)
# Preempt jobs (SIGTERM method test)
#echo "(INFO) Sending SIGTERM signal ${jids[@]}"
#qsig -s SIGTERM ${jids[@]} 

# Launch preemptive jobs
echo "(INFO) Launching $NB_PREEMPTIVE preemptive config (MPI=$((${CONFIG_PREEMPTIVE}*${NB_CORES})))"
prepjob ${CONFIG_PREEMPTIVE} Preemptive ${NB_ITER} 0

for n in $(seq $NB_PREEMPTIVE); do
   [[ ${TEST} -eq 0 ]] && jid=`${command}${QueuePremptive} jobPreemptive.sh`
done