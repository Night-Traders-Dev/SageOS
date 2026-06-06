#ifndef _PTHREAD_H
#define _PTHREAD_H

typedef int pthread_t;
typedef int pthread_mutex_t;
typedef int pthread_cond_t;
typedef int pthread_rwlock_t;
typedef int pthread_attr_t;
typedef int pthread_mutexattr_t;

static inline int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) { (void)thread;(void)attr;(void)start_routine;(void)arg; return -1; }
static inline int pthread_join(pthread_t thread, void **retval) { (void)thread;(void)retval; return -1; }
static inline int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) { (void)mutex;(void)attr; return 0; }
static inline int pthread_mutex_lock(pthread_mutex_t *mutex) { (void)mutex; return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) { (void)mutex; return 0; }
static inline int pthread_mutex_destroy(pthread_mutex_t *mutex) { (void)mutex; return 0; }
static inline pthread_t pthread_self(void) { return 0; }

#endif
