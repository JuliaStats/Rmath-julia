/*
 *  Rmath-julia : allocation and thread-exit cleanup for the per-thread
 *  generator state.  See rmath_tls.h for why the state is on the heap.
 *
 *  This file is specific to Rmath-julia and is not part of R.
 *
 *  Cleanup is automatic: there is no public free function to call, and nothing
 *  for a caller to remember.  C has no portable thread-exit hook, so this uses
 *  the platform's thread-specific-data destructor -- pthread_key_create() on
 *  POSIX, FlsAlloc() on Windows.  The thread-local pointer is kept for fast
 *  access and the *same* pointer is handed to the TSD slot purely so that its
 *  destructor fires when the thread exits.
 *
 *  Windows deliberately does not use pthreads: on mingw-w64 that would resolve
 *  through winpthreads and add a runtime dependency on libwinpthread-1.dll,
 *  which Rmath_jll does not ship (its Yggdrasil recipe declares no
 *  dependencies).  A missing DLL would fail dlopen -- the same class of
 *  breakage this whole change exists to fix.  FlsAlloc is in kernel32.
 *  On every Unix target -lpthread is free: libpthread.so.0 is part of glibc
 *  itself (folded into libc from 2.34), musl's is an empty stub, macOS has it
 *  in libSystem and FreeBSD in base.
 */

#include <stdlib.h>

#include "rmath_tls.h"

/* The library's only static TLS besides sunif.c's seed: one pointer. */
_Thread_local Rmath_tls *Rmath_tls_ptr;

static void rmath_tls_release(void);

/* ------------------------------------------------------------ platform shim */

#ifdef _WIN32

# include <windows.h>

static DWORD fls_index = FLS_OUT_OF_INDEXES;

static void WINAPI on_thread_exit(void *p)
{
    free(p);
    Rmath_tls_ptr = 0;
}

static void tls_setup(void)
{
    fls_index = FlsAlloc(on_thread_exit);
}

static void tls_arm(void *p)
{
    if (fls_index != FLS_OUT_OF_INDEXES)
	FlsSetValue(fls_index, p);
}

#else

# include <pthread.h>

static pthread_key_t tls_key;
static int tls_key_ok;

static void on_thread_exit(void *p)
{
    free(p);
    Rmath_tls_ptr = 0;
}

static void tls_setup(void)
{
    tls_key_ok = (pthread_key_create(&tls_key, on_thread_exit) == 0);
}

static void tls_arm(void *p)
{
    if (tls_key_ok)
	pthread_setspecific(tls_key, p);
}

#endif

/* ---------------------------------------------------------- load and unload */

/* Runs at dlopen, before any thread can call in, so no pthread_once or
 * InitOnceExecuteOnce is needed.  If it somehow did not run, tls_arm() is a
 * no-op and we fall back to the pre-existing behaviour of never releasing:
 * no crash and no wrong answers, just the leak we had before. */
__attribute__((constructor))
static void rmath_tls_startup(void)
{
    tls_setup();
}

/* glibc does not run TSD destructors for the main thread, so its block would
 * otherwise be reported by valgrind/ASan on every single run.  Only the
 * calling thread's block is reachable from here, which is what we want. */
__attribute__((destructor))
static void rmath_tls_shutdown(void)
{
    rmath_tls_release();
}

/* ------------------------------------------------------------------- alloc */

static void rmath_tls_release(void)
{
    if (!Rmath_tls_ptr)
	return;

    free(Rmath_tls_ptr);
    Rmath_tls_ptr = 0;
    tls_arm(0);		/* so the thread-exit destructor cannot double-free */
}

/* Cold path of Rmath_tls_get().  Returns NULL if allocation failed; callers
 * turn that into NaN. */
Rmath_tls *Rmath_tls_alloc(void)
{
    Rmath_tls *t = (Rmath_tls *) calloc(1, sizeof(Rmath_tls));

    if (!t)
	return 0;

    /* calloc zeroed everything; each generator restores its own sentinels. */
    Rmath_rbeta_state_init (&t->rbeta);
    Rmath_rbinom_state_init(&t->rbinom);
    Rmath_rgamma_state_init(&t->rgamma);
    Rmath_rhyper_state_init(&t->rhyper);
    Rmath_rpois_state_init (&t->rpois);

    Rmath_tls_ptr = t;
    tls_arm(t);

    return t;
}
