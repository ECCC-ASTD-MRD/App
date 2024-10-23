#include "App_Timer.h"

void App_TimerInit_f(TApp_Timer* Timer) { App_TimerInit(Timer); }
TApp_Timer* App_TimerCreate_f() { return App_TimerCreate(); }
void App_TimerDelete_f(TApp_Timer* Timer) { App_TimerDelete(Timer); }
void App_TimerStart_f(TApp_Timer* Timer) { App_TimerStart(Timer); }
void App_TimerStop_f(TApp_Timer* Timer) { App_TimerStop(Timer); }
double App_TimerTotalTime_ms_f(const TApp_Timer* Timer) { return App_TimerTotalTime_ms(Timer); }
double App_TimerLatestTime_ms_f(const TApp_Timer* Timer) { return App_TimerLatestTime_ms(Timer); }
double App_TimerTimeSinceStart_ms_f(const TApp_Timer* Timer) { return App_TimerTimeSinceStart_ms(Timer); }


