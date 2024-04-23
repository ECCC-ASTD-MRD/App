#include <omp.h>

#include <App.h>

int main(void) {
    #pragma omp parallel num_threads(8) default(none) shared(stderr)
    {
        App_Log(APP_INFO, "This is an INFO from thread %d/%d\n",  omp_get_thread_num(), omp_get_num_threads());
        App_Log(APP_WARNING, "This is a warning from thread %d/%d\n",  omp_get_thread_num(), omp_get_num_threads());
        App_Log(APP_ERROR, "This is an error from thread %d/%d\n",  omp_get_thread_num(), omp_get_num_threads());
        App_Log(APP_DEBUG, "This is a debug message from thread %d/%d\n",  omp_get_thread_num(), omp_get_num_threads());
    }
    return 0;
}