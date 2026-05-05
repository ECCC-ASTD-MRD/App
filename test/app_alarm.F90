program test_app_fortran

      use app

      implicit none
      integer :: ier

      call app_logstream('stdout')
      app_ptr = App_Init(APP_MASTER, "app_alarm", "test", "alarm signal test", "now")
      call App_Start()

      ier=app_alarm(1)
      call sleep(20)
   
      call app_logstats('FORTRAN')
  
      app_status=app_end(0)
end
