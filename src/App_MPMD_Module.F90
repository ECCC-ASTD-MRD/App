!---------------------------------- LICENCE BEGIN -------------------------------
! GEM - Library of kernel routines for the GEM numerical atmospheric model
! Copyright (C) 1990-2010 - Division de Recherche en Prevision Numerique
!                       Environnement Canada
! This library is free software; you can redistribute it and/or modify it
! under the terms of the GNU Lesser General Public License as published by
! the Free Software Foundation, version 2.1 of the License. This library is
! distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
! without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
! PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
! You should have received a copy of the GNU Lesser General Public License
! along with this library; if not, write to the Free Software Foundation, Inc.,
! 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
!---------------------------------- LICENCE END ---------------------------------

module app_mpmd
   use, intrinsic :: iso_c_binding
   use, intrinsic :: iso_fortran_env
   use mpi
   use app
   implicit none
   save

   enum, BIND(C)
      enumerator :: &
         APP_MPMD_NONE        = 0, &
         APP_MPMD_GEM_ID      = 1, &
         APP_MPMD_IOSERVER_ID = 2, &
         APP_MPMD_IRIS_ID     = 3, &
         APP_MPMD_NEMO_ID     = 4, &

         APP_MPMD_TEST1_ID = 1001, &
         APP_MPMD_TEST2_ID = 1002, &
         APP_MPMD_TEST3_ID = 1003, &
         APP_MPMD_TEST4_ID = 1004, &
         APP_MPMD_TEST5_ID = 1005
   end enum

   interface
      subroutine App_MPMD_Finalize() bind(C, name = 'App_MPMD_Finalize')
         implicit none
      end subroutine App_MPMD_Finalize
   end interface

contains

   subroutine App_MPMD_Init(component_id)
      implicit none
      integer, intent(in) :: component_id

      interface
         function App_MPMD_Init_c(id) result(app_ptr_c) bind(C, name='App_MPMD_Init')
            import :: C_INT, C_PTR
            implicit none
            integer(C_INT), intent(in), value :: id
            type(C_PTR) :: app_ptr_c
         end function App_MPMD_Init_c
      end interface

      type(C_PTR) :: app_ptr
      app_ptr = App_MPMD_Init_c(component_id)
   end subroutine App_MPMD_Init

   !> \return The communicator for all PEs part of the same component as me.
   pure function App_MPMD_GetOwnComm() result(own_comm)
      implicit none
      integer :: own_comm

      interface
         pure function App_MPMD_GetOwnComm_C() result(comm) bind(C, name = 'App_MPMD_GetOwnComm_F')
            import :: C_INT32_T
            implicit none
            integer(C_INT32_T) :: comm
         end function App_MPMD_GetOwnComm_C
      end interface

      own_comm = App_MPMD_GetOwnComm_C()
   end function App_MPMD_GetOwnComm

   !> Retrieve a communicator that encompasses all PEs part of one of the components
   !> in the given list. If the communicator does not already exist, it will be created.
   !> _This function call is collective if and only if the communicator must be created._
   function App_MPMD_GetSharedComm(component_list) result(shared_comm)
      implicit none
      !> The list of components IDs for which we want a shared communicator.
      !> This list *must* contain the component of the calling PE. It may contain
      !> duplicate IDs and does not have to be in a specific order.
      integer, dimension(:), target, intent(in) :: component_list
      integer :: shared_comm
      
      interface
         function App_MPMD_GetSharedComm_C(components, num_components) result(comm) bind(C, name='App_MPMD_GetSharedComm_F')
            import :: C_INT32_T, C_PTR
            implicit none
            ! type(C_PTR),        value, intent(in) :: components
            integer(C_INT32_T),        intent(in) :: components
            integer(C_INT32_T), value, intent(in) :: num_components
            integer(C_INT32_T) :: comm
         end function App_MPMD_GetSharedComm_C
      end interface

      shared_comm = App_MPMD_GetSharedComm_C(component_list(1), size(component_list))
   end function App_MPMD_GetSharedComm


   !> Get the name associated with the given component ID
   function App_MPMD_ComponentIdToName(component_id) result(component_name)
      implicit none

      integer, intent(in) :: component_id
      character(len=:), allocatable :: component_name

      interface
         function id_to_name_c(id) result(name) bind(C, name='App_MPMD_ComponentIdToName')
            import :: C_PTR, C_INT32_T
            implicit none
            integer(C_INT32_T), intent(in), value :: id
            type(C_PTR) :: name
         end function id_to_name_c
      end interface

      component_name = c_to_f_string(id_to_name_c(component_id))
   end function App_MPMD_ComponentIdToName
      
   !> \return Whether the given component is present in this MPMD context
   pure function App_MPMD_HasComponent(component_id) result(has_component)
      implicit none
      integer, intent(in) :: component_id
      logical :: has_component

      interface
         pure function App_MPMD_HasComponent_C(id) result(has_comp) bind(C, name = 'App_MPMD_HasComponent')
            import :: C_INT32_T
            implicit none
            integer(C_INT32_T), intent(in), value :: id
            integer(C_INT32_T) :: has_comp
         end function App_MPMD_HasComponent_C
      end interface

      integer(C_INT32_T) :: has_component_C

      has_component_c = App_MPMD_HasComponent_C(component_id)
      
      has_component = .false.
      if (has_component_c == 1) has_component = .true.

   end function App_MPMD_HasComponent
      
 
 

   function c_to_f_string(c_str) result(f_str)
      implicit none
      type(C_PTR), intent(in) :: c_str
      character(len=:), allocatable :: f_str

      interface
         function c_strlen(str_ptr) bind ( C, name = "strlen" ) result(len)
            import :: C_PTR, C_SIZE_T
            type(C_PTR), value :: str_ptr
            integer(C_SIZE_T)  :: len
        end function c_strlen
      end interface

      character(len=1), dimension(:), pointer :: f_ptr
      integer(INT64) :: num_chars, i

      num_chars = c_strlen(c_str)
      call c_f_pointer(c_str, f_ptr, [num_chars])
      allocate(character(num_chars) :: f_str)

      do i = 1, num_chars
         f_str(i:i) = f_ptr(i)
      end do
   end function c_to_f_string

end module app_mpmd
