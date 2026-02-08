#ifndef APP_ATOMIC_H__
#define APP_ATOMIC_H__

//! \file App_Atomic.h
//! A set of utility functions to perform synchronization and atomic operations. Implements a simple mutex and an
//! atomic integer type.

#include <sys/resource.h>

// #include <immintrin.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

//! Memory store fence
static inline void write_fence(void) {
    // App_Atomic.h:20:22: error: unrecognized instruction mnemonic
    // 20 |     __asm__ volatile("sfence" : : : "memory");
    //    |                      ^
    // __asm__ volatile("sfence" : : : "memory");
}

//! Memory load fence
static inline void read_fence(void) {
    __asm__ volatile("lfence" : : : "memory");
}

//! Memory load+store fence
static inline void full_memory_fence(void) {
    // App_Atomic.h:30:22: error: unrecognized instruction mnemonic
    // 30 |     __asm__ volatile("mfence" : : : "memory");
    //    |                      ^
    // __asm__ volatile("mfence" : : : "memory");
}

//! Acquire the given lock, with the given ID, *without* a memory fence
static inline void acquire_idlock_no_fence(
    volatile int32_t *lock, //!< Address of the location used for locking
    int32_t id              //!< ID of the acquirer. Only that ID will be able to unlock it
) {
    __asm__ volatile("": : :"memory");
    while(__sync_val_compare_and_swap(lock, 0, (id+1)) != 0)
        ;
    __asm__ volatile("": : :"memory");
}

//! Acquire the given lock, with the given ID.
static inline void acquire_idlock(
    volatile int32_t *lock, //!< Address of the location used for locking
    int32_t id              //!< ID of the acquirer. Only that ID will be able to unlock it
) {
    __asm__ volatile("": : :"memory");
    while(__sync_val_compare_and_swap(lock, 0, (id+1)) != 0)
        ;
    full_memory_fence();
}

//! Acquire the given lock, no specific ID, *without* a memory fence
static inline void acquire_lock_no_fence(
    volatile int32_t *lock  //!< Address of the location used for locking
) {
    acquire_idlock_no_fence(lock, 1); // Use 1 as ID
}

//! Acquire the given lock, no specific ID.
static inline void acquire_lock(
    volatile int32_t *lock  //!< Address of the location used for locking
) {
    acquire_idlock(lock, 1); // Use 1 as ID
}

//! Try to acquire the given lock, with a specific ID
//! \return 1 if the lock was successfully acquired by the given ID, 0 if it was already held by someone
static inline int32_t try_acquire_idlock(
    volatile int32_t *lock, //!< Address of the location used for locking
    int32_t id              //!< ID of the acquirer. Only that ID will be able to unlock it
) {
    __asm__ volatile("": : :"memory");
    if (__sync_val_compare_and_swap(lock, 0, (id+1)) != 0) return 0;
    full_memory_fence();
    return 1;
}

//! Release given lock if it has a specific ID (will deadlock if ID is wrong), *without* a memory fence
static inline void release_idlock_no_fence(
    volatile int32_t *lock, //!< Address of the location used for locking
    int32_t id              //!< ID of the acquirer (presumed). If it is locked with another ID, results in a deadlock
) {
    __asm__ volatile("": : :"memory");
    while(__sync_val_compare_and_swap(lock, (id+1), 0) != (id+1))
        ;
    __asm__ volatile("": : :"memory");
}

//! Release lock if it has specific ID (deadlocks if ID is wrong)
static inline void release_idlock(
    volatile int32_t *lock, //!< Address of the location used for locking
    int32_t id              //!< ID of the acquirer (presumed). If it is locked with another ID, results in a deadlock
) {
    full_memory_fence();
    __sync_val_compare_and_swap(lock, (id+1), 0);
    __asm__ volatile("": : :"memory");
}

//! Release given lock without ID (or ID 1; deadlocks if ID is wrong), *without* a fence
static inline void release_lock_no_fence(
    volatile int32_t *lock  //!< Address of the location used for locking
) {
    release_idlock_no_fence(lock, 1);
}

//! Release given lock without ID (or ID 1; deadlocks if ID is wrong)
static inline void release_lock(
    volatile int32_t *lock  //!< Address of the location used for locking
) {
    release_idlock(lock, 1) ;
}

//! Test if lock is held by given ID
//! \return Whether lock is held by [ID]
static inline int32_t is_idlock_taken(
    volatile int32_t *lock, //!< Address of the location used for locking
    int32_t id              //!< ID we are checking for
) {
    return (*lock == (id+1));
}

//! Test if lock is held by anyone
//! \return Whether lock is held by someone
static inline int32_t is_lock_taken(
    volatile int32_t *lock  //!< Address of the location used for locking
) {
    return (*lock != 0);
}

//! Forcefully reset given lock. Should only be used to initialize a lock
static inline void reset_lock(
    volatile int32_t *lock  //!< Address of the location used for locking
) { 
    *lock = 0;
}

//! Try to increment the 32-bit int variable at the given address, if it originally has a certain expected value
//! \return Whether the old value was the same as given to the function AND the variable was incremented
static inline int32_t try_increment(
    volatile int32_t *variable, //!< Address of the variable we want to increment
    int32_t expected_old_value  //!< Value we expect the variable to hold (before incrementing it)
) {
    return (__sync_val_compare_and_swap(variable, expected_old_value, expected_old_value + 1) == expected_old_value);
}

//! Atomic addition operation on an int32.
//! \return The updated value of the variable
static inline int32_t atomic_add_int32(
    volatile int32_t *variable, //!< The variable we are updating
    int32_t increment           //!< How much we want to add to the variable
) {
    int32_t old_value, new_value;
    do {
        old_value = *variable;
        new_value = old_value + increment;
    } while (__sync_val_compare_and_swap(variable, old_value, new_value) != old_value);
    return new_value;
}

#endif
