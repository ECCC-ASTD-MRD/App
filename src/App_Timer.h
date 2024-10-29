#ifndef _App_Timer_h
#define _App_Timer_h

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

//! Timer that can accumulate microsecond intervals
typedef struct {
  uint64_t Start;      //! Timestamp when the timer was started
  uint64_t LatestTime; //! Number of ticks between latest start/stop cycle
  uint64_t TotalTime;  //! How many clock ticks have been recorded (updates every time the timer stops)
  char     String[32]; //! Output representation
} TApp_Timer;

static const clockid_t APP_CLOCK_ID = CLOCK_MONOTONIC;

//! Values that correspond to a reset timer
static const TApp_Timer NULL_TIMER = (const TApp_Timer) {
  .Start = 0,
  .LatestTime = 0,
  .TotalTime = 0
};

//! Get current system time in microseconds, wraps around approximately every year
static inline uint64_t get_current_time_us() {

  struct timespec now;
  clock_gettime(APP_CLOCK_ID, &now);

  // Wraps around every year or so. Not sure why you would need microsecond precision for longer
  const uint64_t now_us = ((uint64_t)now.tv_sec % (1 << 25)) * 1000000 + (uint64_t)now.tv_nsec / 1000;

  return(now_us);
}

static inline void App_TimerInit(TApp_Timer* Timer) {
   if (Timer != NULL)
      *Timer = NULL_TIMER;
}

static inline TApp_Timer* App_TimerCreate() {
   TApp_Timer* timer = (TApp_Timer*)malloc(sizeof(TApp_Timer));
   App_TimerInit(timer);
   return(timer);
}

static inline void App_TimerDelete(TApp_Timer* Timer) {
   if (Timer != NULL) free(Timer);
}

//! Record the current timestamp
static inline void App_TimerStart(TApp_Timer* Timer) {
   Timer->Start = get_current_time_us();
}

//! Increment total time with number of ticks since last start
static inline void App_TimerStop(TApp_Timer* Timer) {
   Timer->LatestTime = get_current_time_us() - Timer->Start;
   Timer->TotalTime += Timer->LatestTime;
}

//! Retrieve the accumulated time in number of milliseconds, as a double
static inline double App_TimerTotalTime_ms(const TApp_Timer* Timer) {
   // If we only count microseconds in a year, this conversion to double does not lose any precision (about 2^31 us/year)
   return(Timer->TotalTime / 1000.0);
}

//! Retrieve the time between the latest start/stop cycle in number of milliseconds, as a double
static inline double App_TimerLatestTime_ms(const TApp_Timer* Timer) {
  // If we only count microseconds in a year, this conversion to double does not lose any precision (about 2^31 us/year)
  return Timer->LatestTime / 1000.0;
}

//! Compute the time between "right now" and the point when this timer was last started
static inline double App_TimerTimeSinceStart_ms(const TApp_Timer* Timer) {
   // If we only count microseconds in a year, this conversion to double does not lose any precision (about 2^31 us/year)
   return((get_current_time_us() - Timer->Start) / 1000.0);
}

#endif

