# Application management library and tools

# Table of Contents
1. [Usage](#usage)
2. [Environment variables](#environment_variables)
3. [Example output log](#example_output_log)
4. [Code Example](#code_example)
5. [Building package](#building_package)

## Usage
This packages manages various standard tasks needed by applications like:

- Program arguments
    - Parsing arguments
    - Pre-defined standard arguments:
```
        -l, --log             Log file (stdout, stderr, file)
            --logsplit        Split log file per MPI rank
        -v, --verbose         Verbose level (ERROR, WARNING, INFO, DEBUG, EXTRA or 0-4)
            --verbosetime     Display time in logs (NONE, DATETIME, TIME, SECOND, MSECOND)
            --verboseutc      Use UTC time
            --verbosecolor    Use color for log messages
        -h, --help            Help info
```
- Logging
   - Standard and uniform way of displaying messages at various levels
   - Manages dependent libraries if they also use this mecanism (App_Log)
   - Optional time information, time step number and color by message type
   - When using MPI, you can select the logging PE, or  if logging from multiple PE, messages are prepended by the PE number
   - Shows count of error and warnings at end/close of log
- Process signal trapping
   - Signal trapping for stopping model on **SIGUSR2**/**SIGTERM**
- Timing functions
- Processes and system information / statistics functions
- Parallel process management OpenMP/MPI
- Single process and MPI memory usage stats
- MPMD MPI process group management

## Environment variables
- **APP_PARAMS**        : List of parameters for the application (instead of giving on command line)
- **APP_VERBOSE**      : Define global verbose level (**ERROR, WARNING, INFO, STAT, TRIVIAL, DEBUG, EXTRA, QUIET**) default: **WARNING**
   - Log level **STAT** can be expanded to include which type of statisics from the **App_LogStat** function you want to be output (**TIME,MEM,CPU**). Default is to output all stats but you can specify which ones as **STAT**:[type]. ex:
      - **STAT:TIME** : Prints the real, user and system time of the process
      - **STAT:MEM**  : Prints the resident, proportional and unique memory setsize 
      - **STAT:CPU**  : Prints the CPU frequency and temperature range 
      - **STAT:TIME:MEM**
- **APP_VERBOSE_[lib]** : Verbose level per library, overrides global (lib=[**RMN, FST, BRP, META, WB, GMM, VGRID, INTERPV, GEOREF, RPNMPI, IRIS, IO, MDLUTIL, DYN, PHY, MIDAS, EER, TDPACK, MACH**])
- **APP_VERBOSE_NOBOX** : Do not display header and footer
- **APP_VERBOSE_COLOR** : Use color in log messages
- **APP_VERBOSE_TIME**  : Display time for each message (**NONE, DATETIME, TIME, SECOND, MSECOND**) default:**NONE**
- **APP_VERBOSE_UTC**   : Display time in UTC
- **APP_VERBOSE_RANK**  : Enable log on a specific rank default:0 (-1=all rank)
- **APP_VERBOSE_THREAD**: Display thread id default:FALSE (be aware that id might not be an ordered 0 to nbthread)
- **APP_TOLERANCE**     : Tolerance level trigerring an exit (**ERROR, SYSTEM, FATAL, QUIET**) default:**QUIET**
- **APP_NOTRAP**        : Disable signal trapping (**SIGTERM, SIGUSR2**)
- **APP_LOG_SPLIT**     : Split log stream/file per MPI PE
- **APP_LOG_STREAM**    : Define log stream/file (**stdout, stderr, filename**) default:stderr
- **APP_LOG_FLUSH**     : Force flush of buffers at every message (default flush only on error)

- **CMCLNG**           : Language to use (**francais, english**)
- **OMP_NUM_THREADS**  : Number of openMP threads (for internal purposes)

## Example output log
```
-------------------------------------------------------------------------------------
Application    : iris 0.0.1 (2023-01-12T19:43:07Z)
Libraries      :
   rmn         : 20.0.0-alpha6-dirty
   vgrid       : 6.9.0
   georef      : 0.1.0

System         : ppp6login-001 (x86_64)
OS             : Linux 4.18.0-372.9.1.el8.x86_64 (#1 SMP Fri Apr 15 22:12:19 EDT 2022)

Start time     : Thu Jan 12 19:43:45 2023
OpenMP threads : 4 (Standard: 201611 -- OpenMP >4.5)
MPI processes  : 13 (Standard: 3.1)
-------------------------------------------------------------------------------------

15       P000 (DEBUG) FST|c_xdfopn: c_xdfopn f->xdf_seq=0
15       P000 (DEBUG) FST|c_xdfopn: fichier existe f->xdf_seq=0
15       P000 (INFO) IRIS|Iris_Init: Defining MDL0 comm with global rank 1 of size 4
16       P000 (INFO) IRIS|Iris_Init: Defining MDL1 comm with global rank 5 of size 4
16       P000 (INFO) IRIS|Iris_Init: Defining MDL2 comm with global rank 9 of size 4
16       P000 (DEBUG) Iris_WaitForSignal: Thread for MDL0 waiting for MPI_Irecv request from model
16       P002 (DEBUG) RMN|getenvc2_: NEWDATE_OPTIONS Not found in environment
16       P002 (TRIVIAL) FST|next_match: Record found at page# 0, record# 2
22       P003 (DEBUG) FST|Read(1) GRID P@ G1_7_1_0N        1442    1021     1  440871200     15728640       123       120      450      984  E 32  O  1001  1002  1003     0
22       P003 (DEBUG) FST|c_fstinfx: iun 1 recherche: datev=0 etiket=[            ] ip1=1001 ip2=1002 ip3=1003 typvar=[  ] nomvar=[^^  ]
22       P003 (TRIVIAL) FST|c_fstinfx: (unit=1) record not found, errcode=-12
55       P000 (DEBUG) Iris_WaitForSignal: Thread processing message from model 0
55       P000 (DEBUG) Iris_WaitForSignal: Thread for MDL(0) is done after finalize call
55       P000 (INFO) IRIS|Iris_Free: Total time spent communicating: 8.612 ms and processing 0.000 ms

-------------------------------------------------------------------------------------
Application    : iris 0.0.1 (2023-01-12T19:43:07Z)

Finish time    : Thu Jan 12 19:43:45 2023
Execution time : 0.2989 seconds (0.56 ms logging)
Resident mem   : 723.2 MB
   Average     : 120.5 MB
   Minimum     : 119.3 MB (rank 3)
   Maximum     : 124.5 MB (rank 0)
   STD         : 1.9 MB
   Above 1 STD : 124.5 MB (rank 0)
Status         : Ok (0 Warnings)
-------------------------------------------------------------------------------------
```

## Code example
```C
int main(int argc, char **argv) {
    char *gridfile = NULL;
    TApp_Arg appargs[]=
        { { APP_CHAR,  &gridfile, 1, "g", "grid", "Input data fields" },
            { APP_NIL } };

    MPI_Init(&argc, &argv);

    int code = 0;
    App_Init(APP_MASTER, MODEL_NAME, VERSION, PROJECT_DESCRIPTION_STRING, GIT_COMMIT_TIMESTAMP);
    if (!App_ParseArgs(appargs, argc, argv, APP_NOARGSFAIL | APP_ARGSLOG)) {
        code = EXIT_FAILURE;
    }

    if (!gridfile) {
        App_Log(APP_ERROR, "No input standard file specified\n");
        exit(EXIT_FAILURE);
    }

    TIris Iris;
    if (!code) {
        App_Start();

        MPI_Comm comm = Iris_Init(&Iris, 0, NULL);

        Model_Init(&Iris, gridfile);
        for(App->Step = 1; App->Step < 20; App->Step++) {

            if (App_IsDone()) {
                // Trapped premption signal
                break;
            }
            Model_Run(&Iris, App->Step, comm);
        }

        if (App_IsDone() && Iris.Rank == 0) {
            App_Log(APP_WARNING, "MDL%d: Would be writing a restart here\n", Iris.ModelNo);
        }

        Iris_Model_Finalize(&Iris);
        App_End(code);
    }
    if (Iris.Rank == 0) cs_fstfrm(OutFID);

    App_Free();

    MPI_Finalize();
}
```


## Building package
### Dependencies

- CMake 3.21+

Note: **cmake_rpn** is included as a submodule.  Please clone with the
**--recursive** flag or run **git submodule update --init --recursive** in the
git repo after having cloned.

### At CMC

Source the right file depending on the architecture you need from the env directory.
This will load the specified compiler and define the ECCI_DATA_DIR variable for the test datasets

- Example for PPP3 and skylake specific architecture:

```bash
. $ECCI_ENV/latest/ubuntu-18.04-skylake-64/intel-19.0.3.199.sh
```

- Example for XC50 on intel-19.0.5

```bash
. $ECCI_ENV/latest/sles-15-skylake-64/intel-19.0.5.281.sh
```

- Example for CMC network and gnu 7.5:

```bash
. $ECCI_ENV/latest/ubuntu-18.04-amd-64/gnu-7.5.0.sh
```

### Build and install

```bash
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=[your install path]-DWITH_OMPI=[TRUE|FALSE] -Drmn_ROOT=[rmnlib location]
make -j 4MP_NUM_THREADS  : Number of openMP threads (for internal purposes)

make test
make install
```
