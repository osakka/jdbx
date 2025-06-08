#ifndef QUICKJS_WRAPPER_H
#define QUICKJS_WRAPPER_H

/* This is a wrapper header to suppress warnings in the QuickJS library headers.
 * We'll silence the unused parameter warnings by adding appropriate pragmas,
 * then include the original QuickJS header.
 */

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

/* Include original QuickJS headers */
#include "quickjs/quickjs.h"
#include "quickjs/quickjs-libc.h"

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* QUICKJS_WRAPPER_H */