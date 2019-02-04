/*
 * $Id: stopwatch.h,v 1.13 2003/10/25 10:40:47 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __STOPWATCH_H
#define __STOPWATCH_H

/*****************************************************************************/

/**
 * @file stopwatch.h
 *
 * @brief Simple portable stopwatch timer implementation
 *
 * Calculates the elapsed time between calls.
 */

/*****************************************************************************/

/**
 * StopWatch structure
 */
typedef struct
{
	unsigned int active : 1;           /**< Current timer state */
	struct timeval start;              /**< Start time */
	struct timeval end;                /**< Stop time */
} StopWatch;

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Alternate interface for initializing a previously allocated StopWatch
 * object.  The suggested usage of this interface is:
 *
 * \code
 * StopWatch sw;
 *
 * stopwatch_init (&sw, ...);
 * stopwatch_finish (&sw);
 * \endcode
 *
 * @param sw  Pointer to a previously allocated StopWatch object.
 */
LIBGIFT_EXPORT
  void stopwatch_init (StopWatch *sw);

/**
 * Release usage of the previously initialized StopWatch object.  This will
 * not free the memory pointed to by sw, as that will be left up to the
 * caller.  See ::stopwatch_init for this interfaces constructor.
 */
LIBGIFT_EXPORT
  void stopwatch_finish (StopWatch *sw);

/**
 * Allocate a new StopWatch object.  See ::stopwatch_init.
 *
 * @param start  If TRUE, automatically start the timer with ::stopwatch_start.
 */
LIBGIFT_EXPORT
  StopWatch *stopwatch_new (BOOL start);

/**
 * Unallocate all memory associated with the StopWatch object.  See
 * ::stopwatch_finish.
 */
LIBGIFT_EXPORT
  void stopwatch_free (StopWatch *sw);

/**
 * Helper identical to calling ::stopwatch_elapsed, and then immediately
 * ::stopwatch_free.  Useful when you want a simple single execution timer
 * instance.
 */
LIBGIFT_EXPORT
  double stopwatch_free_elapsed (StopWatch *sw);

/*****************************************************************************/

/**
 * Start the timer.
 */
LIBGIFT_EXPORT
  void stopwatch_start (StopWatch *sw);

/**
 * Stop the timer.
 */
LIBGIFT_EXPORT
  void stopwatch_stop (StopWatch *sw);

/**
 * Calculate the amount of time elapsed between start and stop.  If the
 * timer is currently active, the time up until this call is calculated.
 *
 * @param sw
 * @param msec Storage location to put number of microseconds.
 *
 * @retval Total elapsed time in seconds (with millisecond precision).
 */
LIBGIFT_EXPORT
  double stopwatch_elapsed (StopWatch *sw, unsigned long *msec);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __STOPWATCH_H */
