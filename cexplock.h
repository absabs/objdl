#ifndef _CEXPLOCK_H_
#define _CEXPLOCK_H_

/* based on cexplock.h form cexp */
/* Wrapper header for mutexes */

/* SLAC Software Notices, Set 4 OTT.002a, 2004 FEB 03
 *
 * Authorship
 * ----------
 * This software (CEXP - C-expression interpreter and runtime
 * object loader/linker) was created by
 *
 *    Till Straumann <strauman@slac.stanford.edu>, 2002-2008,
 * 	  Stanford Linear Accelerator Center, Stanford University.
 *
 * Acknowledgement of sponsorship
 * ------------------------------
 * This software was produced by
 *     the Stanford Linear Accelerator Center, Stanford University,
 * 	   under Contract DE-AC03-76SFO0515 with the Department of Energy.
 * 
 * Government disclaimer of liability
 * ----------------------------------
 * Neither the United States nor the United States Department of Energy,
 * nor any of their employees, makes any warranty, express or implied, or
 * assumes any legal liability or responsibility for the accuracy,
 * completeness, or usefulness of any data, apparatus, product, or process
 * disclosed, or represents that its use would not infringe privately owned
 * rights.
 * 
 * Stanford disclaimer of liability
 * --------------------------------
 * Stanford University makes no representations or warranties, express or
 * implied, nor assumes any liability for the use of this software.
 * 
 * Stanford disclaimer of copyright
 * --------------------------------
 * Stanford University, owner of the copyright, hereby disclaims its
 * copyright and all other rights in this software.  Hence, anyone may
 * freely use it for any purpose without restriction.  
 * 
 * Maintenance of notices
 * ----------------------
 * In the interest of clarity regarding the origin and status of this
 * SLAC software, this and all the preceding Stanford University notices
 * are to remain affixed to any copy or derivative of this software made
 * or distributed by the recipient and are to be affixed to any copy of
 * software made or distributed by the recipient that contains a copy or
 * derivative of this software.
 * 
 * SLAC Software Notices, Set 4 OTT.002a, 2004 FEB 03
 */ 


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__

#include <pthread.h>

typedef pthread_mutex_t CexpLock;

static inline int
cexpLockCreate(CexpLock *lp)
{
	return pthread_mutex_init(lp, NULL);
}

#define cexpLock(l)   		pthread_mutex_lock((&l))
#define cexpUnlock(l) 		pthread_mutex_unlock((&l))
#define cexpLockDestroy(l)	do {} while(0)

#elif defined(__rtems__)

#define HAVE_RTEMS_H


#if defined(HAVE_RTEMS_H)
#include <rtems.h>
#else
/* avoid pulling in <rtems.h> until we can do this in a BSP independent way */
#define rtems_id unsigned long
long rtems_semaphore_obtain();
long rtems_semaphore_release();
long rtems_semaphore_create();
long rtems_semaphore_destroy();

#define rtems_build_name( _C1, _C2, _C3, _C4 ) \
  ( (_C1) << 24 | (_C2) << 16 | (_C3) << 8 | (_C4) )
#define RTEMS_NO_TIMEOUT 				0
#define RTEMS_WAIT		 				0
#define RTEMS_FIFO						0
#define RTEMS_PRIORITY					0x04	/* must be set to get priority inheritance */
#define RTEMS_BINARY_SEMAPHORE			0x10
#define RTEMS_SIMPLE_BINARY_SEMAPHORE	0x20
#define RTEMS_INHERIT_PRIORITY			0x40
#endif

typedef rtems_id CexpLock;
typedef rtems_id CexpEvent;

#define cexpLock(l) 		rtems_semaphore_obtain((l), RTEMS_WAIT, RTEMS_NO_TIMEOUT)
#define cexpEventWait(l) 	rtems_semaphore_obtain((l), RTEMS_WAIT, RTEMS_NO_TIMEOUT)
#define cexpUnlock(l)		rtems_semaphore_release((l))
#define cexpEventSend(l)	rtems_semaphore_release((l))

/* IMPORTANT: use a standard (not simple) binary semaphore that may nest */
static __inline__ int
cexpLockCreate(CexpLock *l)
{
	return
	rtems_semaphore_create(
		rtems_build_name('c','e','x','p'),
		1,/*initial count*/
		RTEMS_PRIORITY|RTEMS_BINARY_SEMAPHORE|RTEMS_INHERIT_PRIORITY,
		0,
		l);
}

static __inline__ int
cexpEventCreate(CexpEvent *pe)
{
	return
	rtems_semaphore_create(
		rtems_build_name('c','e','x','p'),
		0,/*initial count*/
		RTEMS_SIMPLE_BINARY_SEMAPHORE,
		0,
		pe);
}

#define cexpLockDestroy(l) rtems_semaphore_delete((l))
#define cexpEventDestroy(l) rtems_semaphore_delete((l))

#else
#error "thread protection not implemented for this target system"
#endif

#ifdef __cplusplus
}
#endif

#endif
