/*
 * $Id: config.h,v 1.46 2003/10/16 18:50:56 jasta Exp $
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

/*
 * Hand coded Windows version of ./configure generated file.
 */

#ifndef __CONFIG_H
#define __CONFIG_H

/* Name of package */
#define GIFT_PACKAGE "giFT"

/* Version number of package */
#define GIFT_VERSION "0.11.4"

/* libgift version */
#define LIBGIFT_VERSION        \
    ((((0) & 0xff) << 16) |    \
     (((11) & 0xff) << 8) |    \
     (((4) & 0xff)))

#define LIBGIFT_VERSIONSTR "0.11.4"

/* libgiftproto version */
#define LIBGIFTPROTO_MAJOR 0
#define LIBGIFTPROTO_MINOR 11
#define LIBGIFTPROTO_MICRO 4


#undef BUILD_DATE
#define BUILD_DATE __DATE__ " " __TIME__

/* #define DATA_DIR "/usr/local/share/giFT" */

/* Define to 1 if you have the <db3/db.h> header file. */
/* #undef HAVE_DB3_DB_H */

/* Define to 1 if you have the <db4/db.h> header file. */
/* #undef HAVE_DB4_DB_H */

/* Define to 1 if you have the <db.h> header file. */
/* #undef HAVE_DB_H */

/* Define to 1 if you have the <dirent.h> header file. */
/* #define HAVE_DIRENT_H 1 */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #define HAVE_DLFCN_H 1 */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `ftruncate' function. */
/* #define HAVE_FTRUNCATE 1 */

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the `getpagesize' function. */
/* #define HAVE_GETPAGESIZE 1 */

/* Define to 1 if you have the <inttypes.h> header file. */
/* #define HAVE_INTTYPES_H 1 */

/* Define to 1 if you have in_addr_t */
/* #define HAVE_IN_ADDR_T 1 */

/* Define to 1 if you have in_port_t */
/* #define HAVE_IN_PORT_T 1 */

/* Define to 1 if you have the `be' library (-lbe). */
/* #undef HAVE_LIBBE */

/* Define to 1 if you have the `bind' library (-lbind). */
/* #undef HAVE_LIBBIND */

/* Define to 1 if you have the `mingwex' library (-lmingwex). */
/* #undef HAVE_LIBMINGWEX */

/* Define to 1 if you have the `nsl' library (-lnsl). */
/* #define HAVE_LIBNSL 1 */

/* Define to 1 if you have the `socket' library (-lsocket). */
/* #undef HAVE_LIBSOCKET */

/* Define to 1 if you have the `z' library (-lz). */
/* #define HAVE_LIBZ 1 */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <linux/limits.h> header file. */
/* #define HAVE_LINUX_LIMITS_H 1 */

/* Define to 1 if you have the <ltdl.h> header file. */
/* #undef HAVE_LTDL_H */

/* Define to 1 if you have the `madvise' function. */
/* #define HAVE_MADVISE 1 */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have a working `mmap' system call. */
/* #define HAVE_MMAP 1 */

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the `nice' function. */
/* #define HAVE_NICE 1 */

/* Define to 1 if you have the `Perl_eval_pv' function. */
/* #undef HAVE_PERL_EVAL_PV */

/* Define to 1 if you have the `poll' function. */
/* #define HAVE_POLL 1 */

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `signal' function . */
#define HAVE_SIGNAL 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the `snprintf' function. */
/* #define HAVE_SNPRINTF 1 */

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the <stdint.h> header file. */
/* #define HAVE_STDINT_H 1 */

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <stdio.h> header file.
 * Need this for ltdl
 */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
/* #define HAVE_STRINGS_H 1 */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the <syslog.h> header file. */
/* #define HAVE_SYSLOG_H 1 */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
/* #define HAVE_SYS_MMAN_H 1 */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
/* #define HAVE_SYS_TIME_H 1 */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
/* #define HAVE_SYS_WAIT_H 1 */

/* Define to 1 if you have the <unistd.h> header file. */
/* #define HAVE_UNISTD_H 1 */

/* Define to 1 if you have the `vsnprintf' function. */
/* #define HAVE_VSNPRINTF 1 */

/* Define to 1 if you have the `_snprintf' function. */
/* #undef HAVE__SNPRINTF */

/* Define to 1 if you have the `_vsnprintf' function. */
/* #undef HAVE__VSNPRINTF */

/* Name of package */
#undef PACKAGE
#define PACKAGE "giFT"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

#define PLUGIN_DIR ""

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of a `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of a `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* The size of a `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 4

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
/* #define TIME_WITH_SYS_TIME 1 */

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Define to 1 if you want to use getopt */
/* #define USE_GETOPT 1 */

/* #define USE_ID3LIB 1 */

/* #undef USE_IMAGEMAGICK */

/* #undef USE_LIBDB */

/* #define USE_LIBVORBIS 1 */

/* #undef USE_LTDL */

/* #undef USE_PERL */

/* #define USE_ZLIB 1 */

/* Version number of package */
#undef VERSION
#define VERSION "0.10.0"

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to 'uint32' if <netinet/in.h> or <sys/types.h> does not define. */
/* MSVC defines u_long in winsock.h */
#define in_addr_t u_long

/* Define to 'uint16' if <netinet/in.h> or <sys/types.h> does not define. */
/* MSVC defines u_short in winsock.h */
#define in_port_t u_short

/* Define to 'short ' if <sys/types.h> and <stdint.h> do not define. */
/* #define int16_t __int16 */

/* Define to '$ac_cv_gift_int32 ' if <sys/types.h> and <stdint.h> do not
   define. */
/* #define int32_t __int32 */

/* Define to 'char' if <sys/types.h> and <stdint.h> do not define. */
/* #define int8_t __int8 */

/* Define to `int' if <sys/types.h> does not define. */
#define pid_t unsigned int

/* Define to `unsigned' if <sys/types.h> does not define. */
/* MSVC defines in stdio.h */
/* #undef size_t */

/* Define to 'unsigned short ' if <sys/types.h> and <stdint.h> do not define.
   */
#define uint16_t u_short

/* Define to 'unsigned $ac_cv_gift_int32 ' if <sys/types.h> and <stdint.h> do
   not define. */
#define uint32_t u_long

/* Define to 'unsigned char' if <sys/types.h> and <stdint.h> do not define. */
#define uint8_t u_char

/* end of config.h #defines */

/* Define to 1 if you have the <io.h> header file. */
#define HAVE_IO_H 1

#define USE_GETOPT_LONG 1

/*
 * priorities/facilities are encoded into a single 32-bit quantity, where the
 * bottom 3 bits are the priority (0-7) and the top 28 bits are the facility
 * (0-big number).  Both the priorities and the facilities map roughly
 * one-to-one to strings in the syslogd(8) source code.  This mapping is
 * included in this file.
 *
 * priorities (these are ordered)
 */
#define LOG_EMERG   0 /* system is unusable */
#define LOG_ALERT   1 /* action must be taken immediately */
#define LOG_CRIT    2 /* critical conditions */
#define LOG_ERR     3 /* error conditions */
#define LOG_WARNING 4 /* warning conditions */
#define LOG_NOTICE  5 /* normal but significant condition */
#define LOG_INFO    6 /* informational */
#define LOG_DEBUG   7 /* debug-level messages */

/* TODO -- this will be removed when rossta moves back to using pipe for
 * UNIX hosts */
#ifndef AF_LOCAL
# define AF_LOCAL AF_UNIX
#endif

/* speeds compiles as not *every* header is included automatically */
#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN 1
#endif

/* MSVC specifics */
#ifdef _MSC_VER
# define strcasecmp(s1, s2) _stricmp(s1, s2)
# define strncasecmp(s1, s2, count) _strnicmp(s1, s2, count)
# ifndef vsnprintf
#  define vsnprintf _vsnprintf
# endif
# ifndef snprintf
#  define snprintf  _snprintf
# endif

/* from http://www.codeproject.com/tips/aggressiveoptimize.asp */

# ifdef NDEBUG
/* /Og (global optimizations), /Os (favor small code), /Oy (no frame pointers) */
#  pragma optimize("gsy",on)
#   pragma comment(linker,"/RELEASE")
/* Note that merging the .rdata section will result in LARGER exe's if you using
 * MFC (esp. static link). If this is desirable, define _MERGE_RDATA_ in your
 * project. */
#  ifdef _MERGE_RDATA_
#   pragma comment(linker,"/merge:.rdata=.data")
#  endif // _MERGE_RDATA_
#  ifdef _MAX_COMPRESSION_
#   pragma warning( disable : 4078 )
/* these linker commands cause the following warning:
 * LINK : warning LNK4078: multiple ".data" sections found with different
 * attributes (60000020) */
#   pragma comment(linker,"/merge:.text=.data")
#   pragma comment(linker,"/merge:.reloc=.data")
#  endif /* MAX_COMPRESSION */
#  if _MSC_VER >= 1000
/* Only supported/needed with VC6; VC5 already does 0x200 for release builds.
 * Totally undocumented! And if you set it lower than 512 bytes, the program crashes.
 * Either leave at 0x200 or 0x1000 */
#   pragma comment(linker,"/FILEALIGN:0x200")
#  endif /* _MSC_VER >= 1000 */
# endif /* NDEBUG */
#endif /* _MSC_VER */

/* if _WALL is defined, then set the warning level to 3 */
#ifdef _WALL
# ifdef _MSC_VER
#  pragma warning(push, 3)
# endif
#else
/* if _WALL is not defined, then turn off disabled warnings */
# ifdef _MSC_VER
/* warning C4018: '>=' : signed/unsigned mismatch */
#  pragma warning(disable : 4018)
/* warning C4142: benign redefinition of type */
#  pragma warning(disable : 4142)
/* warning C4244: '=' : conversion from 'long ' to 'unsigned short ',
                  possible loss of data */
#  pragma warning(disable : 4244)
/* warning C4761: integral size mismatch in argument; conversion supplied */
#  pragma warning(disable : 4761)
# endif
#endif

#endif /* __CONFIG_H */
