# Preemption Validation Benchmark

Welcome to the Preemption validation Benchmark  

You should have obtained this benchmark from https://github.com/ECCC-ASTD-MRD/App but it is also available through the [GEM Benchmark](https://github.com/ECCC-ASTD-MRD/gem/tree/benchmark-5.3). If you already have built the [GEM Benchmark](https://github.com/ECCC-ASTD-MRD/gem/tree/benchmark-5.3), you can jump to [Run validation benchmark](#run-validation-benchmark)

# Test description

* This test verifies that the proposed batch system can gracefully preempt running jobs in order to free resources for a high priority.
* The test consists of filling one Supercomputer cluster with “app” processes, until 10 or more jobs are waiting in the queue, and then submitting high priority jobs.  
* The initial jobs filling the cluster have to use 1 node each and the 2 preemptive jobs have to use 10% of the nodes available on the cluster each.

# Requirements

* C compiler. Theses codes have been tested with compilers from GNU and Intel OneAPI (classic and llvm based)
* An MPI implementation such as OpenMPI, MPICH or Intel MPI (with development package)
* OpenMP support
* CMake (version >= 3.20)

# Build

## Compiler specifics

* Compiler specific definitions and flags are defined within the ```cmake_rpn``` submodule of each code repository. If you need to change or add any, 
you can add or modify the rules into `[git source path]/cmake_rpn/modules/ec_compiler_presets/default/[architecture]/`

## Build base library (App)
* Note that this package is part of the [GEM Benchmark](https://github.com/ECCC-ASTD-MRD/gem/tree/benchmark-5.3), and will be built and installed when you build the librmn component of that benchmark 

```bash
git clone git@github.com:ECCC-ASTD-MRD/App.git
cd App
git submodule update --init
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=[App install directory] ..
make install
export PATH=[App install directory]/bin
```

# Run validation benchmark

- The application (``app``) to use in the submission scripts will be in the /bin install directory specified at the build/install step (``-DCMAKE_INSTALL_PREFIX=[App install directory]``). 
- You will have to pass the submission time (as Unix seconds since January 1st, 1970) with parameter -q to the ``app`` application 
```bash
app -q $(date +%s)
```
- An example of the validation script (``app-preemption-bench.sh``) that will be used by ECCC and that you can also use and adapt to your technological choice of scheduler and premption methods is also available in the same directory. This script call will be:
```bash
/home/ords/cmdd/cmds/nil000/ssm/workspace/App/bin/app-preemption-bench.sh -n [number of nodes on system] -f 10 -B 0
```
