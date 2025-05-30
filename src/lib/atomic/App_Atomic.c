#include "App_Atomic.h"

//! \file App_Atomic.c
//! Mostly just Fortran-callable implementations of the corresponding C functions (from App_Atomic.h)

void    acquire_idlock_F(volatile int32_t *lock, int32_t id) { acquire_idlock(lock, id); }
void    release_idlock_F(volatile int32_t *lock, int32_t id) { release_idlock(lock, id); }
int32_t try_acquire_idlock_F(volatile int32_t *lock, int32_t id) { return try_acquire_idlock(lock, id); }
int32_t is_idlock_taken_F(volatile int32_t *lock, int32_t id) { return is_idlock_taken(lock, id); }
int32_t is_lock_taken_F(volatile int32_t *lock) { return is_lock_taken(lock); }
void    reset_lock_F(volatile int32_t *lock) { reset_lock(lock); }

int32_t try_update_int32_F(volatile int32_t *variable, int32_t old_value, int32_t new_value) {
  return __sync_val_compare_and_swap(variable, old_value, new_value) == old_value;
}

int32_t atomic_add_int32_F(volatile int32_t *variable, int32_t increment) {
  return atomic_add_int32(variable, increment);
}
