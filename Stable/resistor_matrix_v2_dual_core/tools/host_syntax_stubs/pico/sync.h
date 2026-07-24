#pragma once
#include <stdint.h>
struct spin_lock_t{};
struct semaphore_t{};
inline uint32_t spin_lock_claim_unused(bool){return 0;}
inline spin_lock_t* spin_lock_init(uint32_t){static spin_lock_t x; return &x;}
inline uint32_t spin_lock_blocking(spin_lock_t*){return 0;}
inline void spin_unlock(spin_lock_t*,uint32_t){}
inline void sem_init(semaphore_t*, int, int){}
inline void sem_release(semaphore_t*){}
inline bool sem_acquire_timeout_ms(semaphore_t*, uint32_t){return true;}
inline void __sev(){}
inline void __wfe(){}
inline void __dmb(){}
