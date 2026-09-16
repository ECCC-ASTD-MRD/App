#include <stdio.h>
#include <string.h>
#include <App.h>


int main(int argc, char * argv[]) {
    if (argc < 1 && argc > 3) {
        App_Log(APP_FATAL, "Invalid command line syntax! (%s [--full] [Tolerance_Level])\n");
    }

    char full = 0;
    if (argc >= 2) {
        if (strcmp("--full", argv[1]) == 0) {
            full = 1;
            if (argc == 3) {
                App_ToleranceLevel(argv[2]);
            }
        } else {
            App_ToleranceLevel(argv[1]);
        }
    }

    if (full) {
        App_Init(APP_MASTER, argv[0], "0.0.0", "Test of the various message levels", "2026-09-16");
        App_Start();
    }

    // The default log level is APP_QUIET which will not cause the execution to abort
    // unless a message with the APP_FATAL level is written
    // Also, messages with level APP_ERROR will not cause a non-zero result code

    // Setting any tolerance level will cause the end box to be printed

    // Setting the tolerance level to APP_ERROR will cause a non-zero result code

    App_Log(APP_INFO, "An info level message\n");
    App_Log(APP_WARNING, "An warning level message\n");
    // Logging an error will not cause a non-zero return code without calling App_And(-1)
    App_Log(APP_ERROR, "An error level message\n");
    // App_Log(APP_SYSTEM, "An system level message\n");
    // App_Log(APP_FATAL, "An fatal level message\n");
    App_Log(APP_INFO, "After a error level message\n");

    if (full) {
        return App_End(-1);
    }

    return 0;
}
