/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LINKER_DEBUG_H_
#define _LINKER_DEBUG_H_

#include <stdio.h>

/* WARNING: For linker debugging only.. Be careful not to leave  any of
 * this on when submitting back to repository */
#define LINKER_DEBUG         1
#define TRACE_DEBUG          1
#define DO_TRACE_LOOKUP      0
#define DO_TRACE_RELO        0
#define TIMING               0

/*********************************************************************
 * You shouldn't need to modify anything below unless you are adding
 * more debugging information.
 *
 * To enable/disable specific debug options, change the defines above
 *********************************************************************/


/*********************************************************************/
#undef TRUE
#undef FALSE
#define TRUE                 1
#define FALSE                0


#define __PRINTVF(v,f,x...)   do {                                        \
        (debug_verbosity > (v)) && (printf(x), ((f) && fflush(stdout))); \
    } while (0)
#if LINKER_DEBUG
extern int debug_verbosity;
#define _PRINTVF(v,f,x...)    __PRINTVF(v,f,x)
#else /* !LINKER_DEBUG */
#define _PRINTVF(v,f,x...)   do {} while(0)
#endif /* LINKER_DEBUG */

#define PRINT(x...)          _PRINTVF(-1, FALSE, x)
#define INFO(x...)           _PRINTVF(0, TRUE, x)
#define TRACE(x...)          _PRINTVF(1, TRUE, x)
#define WARN(fmt,args...)    \
        _PRINTVF(-1, TRUE, "%s:%d| WARNING: " fmt, __FILE__, __LINE__, ## args)
#define ERROR(fmt,args...)   \
        __PRINTVF(-1, TRUE, "%s:%d| ERROR: " fmt, __FILE__, __LINE__, ## args)

#if TRACE_DEBUG
#define DEBUG(x...)          _PRINTVF(2, TRUE, "DEBUG: " x)
#else /* !TRACE_DEBUG */
#define DEBUG(x...)          do {} while (0)
#endif /* TRACE_DEBUG */

#if LINKER_DEBUG
#define TRACE_TYPE(t,x...)   do { if (DO_TRACE_##t) { TRACE(x); } } while (0)
#else  /* !LINKER_DEBUG */
#define TRACE_TYPE(t,x...)   do {} while (0)
#endif /* LINKER_DEBUG */

#if TIMING
#undef WARN
#define WARN(x...)           do {} while (0)
#endif /* TIMING */

#endif /* _LINKER_DEBUG_H_ */
