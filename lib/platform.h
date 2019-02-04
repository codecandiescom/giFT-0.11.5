/*
 * $Id: platform.h,v 1.23 2003/10/16 18:50:55 jasta Exp $
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

#ifndef __PLATFORM_H
#define __PLATFORM_H

/*****************************************************************************/

/**
 * @file platform.h
 *
 * @brief Platform specific routines.
 *
 * Contains routines that are specific to a platform (*nix, Windows, etc).
 * You should be aware that you are expected to call \ref platform_init
 * before using any routines held within libgiFT.
 */

/*****************************************************************************/

#include <sys/stat.h> /* stat macros */

#ifdef WIN32
# ifndef FD_SETSIZE
#  define FD_SETSIZE 1024
# endif
# include <winsock2.h>	/* include *before* windows */
# include <windows.h>
#endif /* !WIN32 */

/*****************************************************************************/
/* definitions for platform_child_proc */

struct _sproc_data;

#define PARENT_FUNC(func) int func (SubprocessData *subproc, void *udata)
typedef int (*ParentFunc) (struct _sproc_data *sdata, void *udata);

#define CHILD_FUNC(func) int func (SubprocessData *subproc, void *udata)
typedef int (*ChildFunc) (struct _sproc_data *sdata, void *udata);

#ifndef WIN32
# define SUBPROCESS(func) int func (void *param)
#else /* WIN32 */
# define SUBPROCESS(func) DWORD WINAPI func (LPVOID param)
#endif /* !WIN32 */

/**
 * Structure containing data passed between parent and child processes.
 */
typedef struct _sproc_data
{
	int         fd;
	ChildFunc   cfunc;
	ParentFunc  pfunc;
	char       *data;
	size_t      len;
	size_t      data_len;
	void       *udata;
} SubprocessData;

/*****************************************************************************/

#ifndef __GNUC__
# define __PRETTY_FUNCTION__ ""
#endif

#ifndef S_ISDIR
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISFIFO
# define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#endif

#ifndef S_ISCHR
# define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#endif

#ifndef S_ISBLK
# define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#endif

#ifndef S_ISREG
# define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

/*****************************************************************************/

EXTERN_C_BEGIN

/*****************************************************************************/

/**
 * Contains installed data, such as mime.types.
 *
 * UNIX:    this is defined by configure as DATA_DIR
 * Windows: The plugin_dir + /data
 */
LIBGIFT_EXPORT
  char *platform_data_dir (void);

/**
 * Contains all dynamically loaded plugins that this giFT installation has
 * available.
 *
 * UNIX:    this is defined by configure as PLUGIN_DIR
 * Windows: Searches for the first valid directory in:
 *          1. HKEY_LOCAL_MACHINE\Software\giFT\giFT\instpath registry key
 *          2. Directory where giFT started from
 *          3. Current directory
 */
LIBGIFT_EXPORT
  char *platform_plugin_dir (void);

/**
 * Contains the user's 'home' directory.
 *
 * UNIX:    assigned to $HOME or /home/$USER
 * Windows: Searches for the first valid directory in:
 *          1. HKEY_CURRENT_USER\Software\giFT\giFT\instpath registry key
 *          2. %HOME% directory, if %HOME% is defined in the environment.
 *          3. Directory where giFT started from.
 *          4. Current directory.
 */
LIBGIFT_EXPORT
  char *platform_home_dir (void);

/**
 * Contains all user preferences (.conf files).
 *
 * UNIX:    assigned to $HOME/.giFT
 * Windows: Searches for the first valid directory in:
 *          1. home_dir
 *          2. home_dir + /.giFT
 */
LIBGIFT_EXPORT
  char *platform_local_dir (void);

/**
 * Platform specific initialization.
 *
 * Must be called before calling any other functions in this library.  See
 * ::libgift_init for a more appropriate and complete wrapper.
 *
 * @param home_dir    Optional home_dir hint.
 * @param local_dir   Optional local_dir hint.
 * @param data_dir    Optional data_dir hint.
 * @param plugin_dir  Optional plugin_dir hint.
 *
 * @return Boolean success or failure.  Failure to initialize the platform
 *         will result in several systems being unusable.  You should not
 *         proceed with execution.
 */
LIBGIFT_EXPORT
  BOOL platform_init (const char *home_dir, const char *local_dir,
                      const char *data_dir, const char *plugin_dir);

/**
 * Platform specific cleanup.  Frees any memory that was allocated by
 * ::platform_init.
 *
 * Please note that ::platform_cleanup should be called on program
 * termination.
 */
LIBGIFT_EXPORT
  void platform_cleanup (void);

/**
 * Emulates *nix's gettimeofday function.
 *
 * @param tv      Time structure.
 * @param unused  Not used, doesn't matter what you pass.
 */
LIBGIFT_EXPORT
  int platform_gettimeofday (struct timeval *tv, void *unused);

/**
 * Spawn's a child process. In *nix, fork is used, in Windows, a thread is
 * used.
 *
 * @param cfunc  Pointer to child function.
 * @param pfunc  Pointer to parent function.
 * @param udata
 *
 * @return Boolean success or failure.
 */
LIBGIFT_EXPORT
  int platform_child (ChildFunc cfunc, ParentFunc pfunc, void *udata);

/**
 * Send a message to a parent process from a child process.
 *
 * @param sdata  SubprocessData of the child context.
 * @param msg    Message to send.
 * @param len    Size of the message to send.
 *
 * @return See send(2).
 */
LIBGIFT_EXPORT
  int platform_child_sendmsg (SubprocessData *sdata, char *msg, size_t len);

/**
 * Receive a message from a child process in a parent process.  The data is
 * returned in sdata->data, and the length in sdata->len.
 *
 * @param sdata  SubprocessData of the child process.
 *
 * @return See recv(2)
 */
LIBGIFT_EXPORT
  int platform_child_recvmsg (SubprocessData *sdata);

/**
 * Access an HTTP_USER_AGENT string describing the giFT version and OS
 * version.  Uhm, this really wasn't very well throught out and you should
 * definitely avoid using this function.
 *
 * @return Pointer to local memory, I believe.  Check for yourself if you
 *         really think you should be using this.
 */
LIBGIFT_EXPORT
  char *platform_version (void);

/**
 * Access the error code of the last system error.  This uses GetLastError()
 * on Windows and errno for everything else.
 *
 * @note We should not be using an unsigned long return value here and I
 *       cannot think why we are (but there must be a reason, right?).  rasa,
 *       please check this and get back to me.
 */
LIBGIFT_EXPORT
  unsigned long platform_errno (void);

/**
 * Access the error message of the last system error.  On Windows ,
 * FormatMessage() and GetLastError() will be used.  Otherwise the standard
 * strerror call will be used.
 *
 * @note You may want to consider ::platform_net_error.
 */
LIBGIFT_EXPORT
  char *platform_error (void);

/**
 * Access the error code of the last socket error.  This function is
 * implemented only to work around Windows non-conformity.
 */
LIBGIFT_EXPORT
  unsigned long platform_net_errno (void);

/**
 * Similar to ::platform_error, but will use ::platform_net_errno to retrieve
 * the error number.
 */
LIBGIFT_EXPORT
  char *platform_net_error (void);

/*****************************************************************************/

EXTERN_C_END

/*****************************************************************************/

#endif /* __PLATFORM_H */
