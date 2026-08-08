/*
 * Single-threaded POSIX threads shim for the Xbox 360.
 *
 * The Xbox 360 exposes no thread API to the game other than kernel
 * ExCreateThread, so nothing in the C library can spawn threads.  This
 * library provides link-time implementations of the POSIX threads API (and
 * a few small POSIX services the console's Mesa build references) that
 * assume a single thread: locks never contend, condition variables never
 * wait, and pthread_create() fails with EAGAIN so callers can fall back to
 * single-threaded operation.
 */

#ifndef _POSIX_THREADS
#define _POSIX_THREADS
#define _POSIX_BARRIERS
#define _POSIX_READER_WRITER_LOCKS
#define _POSIX_TIMEOUTS
#define _UNIX98_THREAD_MUTEX_ATTRIBUTES
#define _POSIX_CLOCK_SELECTION
#define _POSIX_MONOTONIC_CLOCK
#define _POSIX_TIMERS
#endif

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#define PTHREAD_SHIM_KEYS 32

static void *shim_tls_values[PTHREAD_SHIM_KEYS];
static unsigned shim_keys_used;

/* ------------------------------ threads ------------------------------ */

pthread_t
pthread_self(void)
{
    return (pthread_t) 0;
}

int
pthread_equal(pthread_t a, pthread_t b)
{
    return a == b;
}

int
pthread_create(pthread_t *thread, const pthread_attr_t *attr,
               void *(*start_routine)(void *), void *arg)
{
    (void) thread;
    (void) attr;
    (void) start_routine;
    (void) arg;
    return EAGAIN;
}

int
pthread_join(pthread_t thread, void **retval)
{
    (void) thread;
    (void) retval;
    return EINVAL;
}

int
pthread_detach(pthread_t thread)
{
    (void) thread;
    return 0;
}

void
pthread_exit(void *retval)
{
    (void) retval;
    for (;;) {
    }
}

/* ------------------------------- once ------------------------------- */

int
pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
    if (!once_control->init_executed) {
        init_routine();
        once_control->init_executed = 1;
    }
    return 0;
}

/* ------------------------------- mutex ------------------------------ */

int
pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
    (void) attr;
    *mutex = (pthread_mutex_t) 0;
    return 0;
}

int
pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    (void) mutex;
    return 0;
}

int
pthread_mutex_lock(pthread_mutex_t *mutex)
{
    (void) mutex;
    return 0;
}

int
pthread_mutex_trylock(pthread_mutex_t *mutex)
{
    (void) mutex;
    return 0;
}

int
pthread_mutex_timedlock(pthread_mutex_t *mutex,
                        const struct timespec *abstime)
{
    (void) mutex;
    (void) abstime;
    return 0;
}

int
pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    (void) mutex;
    return 0;
}

int
pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int
pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
    (void) attr;
    return 0;
}

int
pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
    (void) attr;
    (void) type;
    return 0;
}

int
pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type)
{
    (void) attr;
    *type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

/* ---------------------------- condition ----------------------------- */

int
pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    (void) attr;
    *cond = (pthread_cond_t) 0;
    return 0;
}

int
pthread_cond_destroy(pthread_cond_t *cond)
{
    (void) cond;
    return 0;
}

int
pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    (void) cond;
    (void) mutex;
    return 0;
}

int
pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                       const struct timespec *abstime)
{
    (void) cond;
    (void) mutex;
    (void) abstime;
    return 0;
}

int
pthread_cond_signal(pthread_cond_t *cond)
{
    (void) cond;
    return 0;
}

int
pthread_cond_broadcast(pthread_cond_t *cond)
{
    (void) cond;
    return 0;
}

int
pthread_condattr_init(pthread_condattr_t *attr)
{
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int
pthread_condattr_destroy(pthread_condattr_t *attr)
{
    (void) attr;
    return 0;
}

int
pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id)
{
    (void) attr;
    (void) clock_id;
    return 0;
}

/* ------------------------------ rwlock ------------------------------ */

int
pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
    (void) attr;
    *rwlock = (pthread_rwlock_t) 0;
    return 0;
}

int
pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
    (void) rwlock;
    return 0;
}

int
pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
    (void) rwlock;
    return 0;
}

int
pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
    (void) rwlock;
    return 0;
}

int
pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
    (void) rwlock;
    return 0;
}

int
pthread_rwlock_trylock(pthread_rwlock_t *rwlock)
{
    (void) rwlock;
    return 0;
}

int
pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock)
{
    (void) rwlock;
    return 0;
}

/* ------------------------------ barrier ----------------------------- */

int
pthread_barrier_init(pthread_barrier_t *barrier,
                     const pthread_barrierattr_t *attr, unsigned count)
{
    (void) attr;
    (void) count;
    *barrier = (pthread_barrier_t) 0;
    return 0;
}

int
pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    (void) barrier;
    return 0;
}

int
pthread_barrier_wait(pthread_barrier_t *barrier)
{
    (void) barrier;
    return PTHREAD_BARRIER_SERIAL_THREAD;
}

/* -------------------------------- key ------------------------------- */

int
pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
    (void) destructor;
    if (shim_keys_used >= PTHREAD_SHIM_KEYS)
        return ENOMEM;
    *key = (pthread_key_t) shim_keys_used;
    shim_keys_used++;
    return 0;
}

int
pthread_key_delete(pthread_key_t key)
{
    if ((unsigned) key >= shim_keys_used)
        return EINVAL;
    shim_tls_values[key] = NULL;
    return 0;
}

void *
pthread_getspecific(pthread_key_t key)
{
    if ((unsigned) key < PTHREAD_SHIM_KEYS)
        return shim_tls_values[key];
    return NULL;
}

int
pthread_setspecific(pthread_key_t key, const void *value)
{
    if ((unsigned) key >= PTHREAD_SHIM_KEYS)
        return EINVAL;
    shim_tls_values[key] = (void *) value;
    return 0;
}

int
pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
    (void) how;
    (void) set;
    (void) oset;
    return 0;
}

/* ----------------------------- clocks ------------------------------ */

int
pthread_getcpuclockid(pthread_t thread, clockid_t *clock_id)
{
    (void) thread;
    *clock_id = CLOCK_REALTIME;
    return 0;
}

int
clock_gettime(clockid_t clock_id, struct timespec *ts)
{
    struct timeval tv;

    (void) clock_id;
    gettimeofday(&tv, NULL);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
    return 0;
}

int
clock_getres(clockid_t clock_id, struct timespec *res)
{
    (void) clock_id;
    res->tv_sec = 0;
    res->tv_nsec = 1000;
    return 0;
}

int
clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *rqtp,
                struct timespec *rmtp)
{
    (void) clock_id;
    (void) flags;
    (void) rqtp;
    (void) rmtp;
    return 0;
}

int
nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
    (void) rqtp;
    (void) rmtp;
    return 0;
}

int
usleep(useconds_t useconds)
{
    (void) useconds;
    return 0;
}

int
sched_yield(void)
{
    return 0;
}
int
pthread_setcancelstate(int state, int *oldstate)
{
    (void) state;
    if (oldstate)
        *oldstate = 0;
    return 0;
}
