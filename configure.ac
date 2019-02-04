###############################################################################
##
## $Id: configure.ac,v 1.156 2003/12/26 21:37:00 jasta Exp $
##
## For anyone reading this, please know that there is a lot of work to be
## done here to improve our environment checking.  Patches are greatly
## encouraged.
##
## Some sections of this file were copied from other [now] unknown sources.
##
## This program is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation; either version 2, or (at your option) any
## later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
###############################################################################

dnl ###########################################################################
dnl Initialize autoconf...
dnl ###########################################################################

AC_PREREQ(2.52)

AC_INIT(giFT, 0.11.5)
AC_CONFIG_SRCDIR([src/giftd.h])

dnl ###########################################################################
dnl Setup giFT-specific substitution variables
dnl ###########################################################################

dnl
dnl GIFT_MKVER(PFX,MAJOR,MINOR,MICRO)
dnl
dnl Sets PFX_{MAJOR,MINOR,MICRO} and joins them using a '.' character to
dnl set PFX_VERSION.  PFX_VERSIONSTR is also defined to create a C string.
dnl
dnl Also handles AC_SUBST calls for the assigned variables.
dnl
AC_DEFUN([GIFT_MKVER],
         [AS_VAR_SET($1_MAJOR, [$2])
          AS_VAR_SET($1_MINOR, [$3])
          AS_VAR_SET($1_MICRO, [$4])
          AS_VAR_SET($1_VERSION, $2.$3.$4)
          AS_VAR_SET($1_VERSIONSTR, "\"$2.$3.$4\"")

          AC_SUBST($1_MAJOR)
          AC_SUBST($1_MINOR)
          AC_SUBST($1_MICRO)
          AC_SUBST($1_VERSION)
          AC_SUBST($1_VERSIONSTR)
          ])

GIFT_MKVER(GIFTD,        0, 11, 5)
GIFT_MKVER(LIBGIFT,      0, 11, 5)
GIFT_MKVER(LIBGIFTPROTO, 0, 11, 5)

# This is required because we dont hook into AC_INIT and our clean
# rules are garbage.
LIBGIFT_CLEAR_CONFIG

dnl ###########################################################################
dnl Initialize automake...
dnl ###########################################################################

AM_INIT_AUTOMAKE([dist-bzip2])
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

dnl ###########################################################################
dnl Initialize autoheader...
dnl ###########################################################################

AH_VERBATIM([BUILD_DATE],      [#undef BUILD_DATE])
AH_VERBATIM([USE_LTDL],        [#undef USE_LTDL])
AH_VERBATIM([USE_PERL],        [#undef USE_PERL])
AH_VERBATIM([USE_LIBVORBIS],   [#undef USE_LIBVORBIS])
AH_VERBATIM([USE_IMAGEMAGICK], [#undef USE_IMAGEMAGICK])
AH_VERBATIM([USE_LIBMAGIC],    [#undef USE_LIBMAGIC])

dnl ###########################################################################
dnl Build information...
dnl ###########################################################################

BUILD_DATE="`date`"
AC_DEFINE_UNQUOTED(BUILD_DATE, "$BUILD_DATE")

dnl ###########################################################################
dnl Checks for the necessary build tools...
dnl ###########################################################################

AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_INSTALL
AC_PATH_PROG(SED, sed)

# Not exactly required for normal builds, but needed to generate the
# documentation that goes into the dist tarballs.
AC_PATH_PROG(PERL, perl)
AC_PATH_PROG(XSLTPROC, xsltproc)
AM_CONDITIONAL(BUILD_DOCS, test x$XSLTPROC != x -a x$PERL != x)

AM_DISABLE_STATIC
AC_PROG_LIBTOOL

dnl ###########################################################################
dnl Checks for depended libraries...
dnl ###########################################################################

AC_CHECK_LIB(nsl, gethostbyname)
AC_CHECK_LIB(socket, socket)
AC_CHECK_LIB(mingwex, opendir)
AC_CHECK_LIB(bind, inet_ntoa)
AC_CHECK_LIB(be, openlog)

# I think this is used for mingw32?  Can anyone confirm?
GIFT_CHECK_WINSOCK_LIBS

dnl ###########################################################################
dnl Checks for header files...
dnl ###########################################################################

AC_HEADER_DIRENT                       # POSIX: <dirent.h>
AC_HEADER_STDC                         # C89: <stdlib.h>, <stdio.h>, ...
AC_HEADER_SYS_WAIT                     # POSIX: <sys/wait.h>

AC_CHECK_HEADERS(dirent.h \
                 fcntl.h \
                 getopt.h \
                 limits.h \
                 linux/limits.h \
		 stdint.h \
                 sys/mman.h \
                 sys/time.h \
                 syslog.h \
                 signal.h \
                 unistd.h \
                 inttypes.h \
                 io.h)

dnl ###########################################################################
dnl Checks for typedefs, structures, and compiler characteristics...
dnl ###########################################################################

# Not sure we use the const check here?
AC_C_CONST
AC_HEADER_TIME
AC_STRUCT_TM

# Translate checks for certain headers to lib/giftconfig.h.
AS_IF([test "${ac_cv_header_time}" = yes],
      [LIBGIFT_DEFINE(TIME_WITH_SYS_TIME)],
      [])

AS_IF([test "${ac_cv_header_sys_time_h}" = yes],
      [LIBGIFT_DEFINE(HAVE_SYS_TIME_H)],
      [])

AS_IF([test "${ac_cv_struct_tm}" = sys/time.h],
      [LIBGIFT_DEFINE(TM_IN_SYS_TIME)],
      [])

# Check endianness, mostly for protocol plugin convenience.
AC_C_BIGENDIAN

AS_IF([test "${ac_cv_c_bigendian}" = yes],
      [LIBGIFT_DEFINE(WORDS_BIGENDIAN)],
      [])

# Determine size and/or presence of certain primitive types for the ultimate
# purpose of defining size-specific types if C99's stdint.h does not provide.
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(void *)

# Translate to lib/giftconfig.h
LIBGIFT_DEFINE_UNQUOTED(SIZEOF_SHORT, AS_VAR_GET(ac_cv_sizeof_short))
LIBGIFT_DEFINE_UNQUOTED(SIZEOF_INT, AS_VAR_GET(ac_cv_sizeof_int))
LIBGIFT_DEFINE_UNQUOTED(SIZEOF_LONG, AS_VAR_GET(ac_cv_sizeof_long))

# Actually assign size-specific types.
if test "${ac_cv_sizeof_int}" = "4" ; then
   AS_VAR_SET(ac_cv_gift_int32, int)
elif test "${ac_cv_sizeof_long}" = "4"; then
   AS_VAR_SET(ac_cv_gift_int32, long)
else
   AC_MSG_ERROR(No 32-bit datatype found - please report this to the developers)
fi

LIBGIFT_CHECK_INTTYPE(uint8_t, unsigned char)
LIBGIFT_CHECK_INTTYPE(int8_t, signed char)
LIBGIFT_CHECK_INTTYPE(uint16_t, unsigned short)
LIBGIFT_CHECK_INTTYPE(int16_t, signed short)
LIBGIFT_CHECK_INTTYPE(uint32_t, unsigned AS_VAR_GET(ac_cv_gift_int32))
LIBGIFT_CHECK_INTTYPE(int32_t, AS_VAR_GET(ac_cv_gift_int32))

LIBGIFT_DEFINE(BOOL, int)
LIBGIFT_DEFINE(TRUE, 1)
LIBGIFT_DEFINE(FALSE, 0)

# POSIX (or is it something else?) defines in_addr_t and in_port_t for
# IPv4 network addresses and port numbers, and we will inherit this convention
# for libgift.
LIBGIFT_CHECK_TYPE(in_addr_t, uint32_t,
                   [Define to 'uint32_t' if <netinet/in.h> and <sys/types.h> do not define],
                   [#include <sys/types.h>
                    #include <netinet/in.h>
                    ])

LIBGIFT_CHECK_TYPE(in_port_t, uint16_t,
                   [Define to 'uint16_t' if <netinet/in.h> and <sys/types.h> do not define],
                   [#include <sys/types.h>
                    #include <netinet/in.h>
                    ])

# More POSIX goodness.  Actually, I'm not sure about off_t and size_t.
LIBGIFT_CHECK_TYPE(pid_t, int)
LIBGIFT_CHECK_TYPE(off_t, long)
LIBGIFT_CHECK_TYPE(size_t, unsigned int)

# We need either getopt() or getopt_long().  No exceptions.
AC_CACHE_CHECK([for getopt], ac_cv_func_getopt,
                 [AC_LINK_IFELSE(
                   [AC_LANG_PROGRAM(
                     [AC_INCLUDES_DEFAULT(
                       [#include <unistd.h>
                        ])],
                     [getopt (0, NULL, "")])],
                   [AS_VAR_SET(ac_cv_func_getopt, yes)],
                   [AS_VAR_SET(ac_cv_func_getopt, no)])])

AS_IF([test AS_VAR_GET(ac_cv_func_getopt) = yes],
      [AC_DEFINE_UNQUOTED(
        [USE_GETOPT], [1],
        [Define to 1 if you want to use getopt])],
      [])

AC_CACHE_CHECK([for getopt_long], ac_cv_func_getopt_long,
                 [AC_LINK_IFELSE(
                   [AC_LANG_PROGRAM(
                     [AC_INCLUDES_DEFAULT(
                       [#include <unistd.h>
                        ])],
                     [getopt_long (0, NULL, "", NULL, NULL)])],
                   [AS_VAR_SET(ac_cv_func_getopt_long, yes)],
                   [AS_VAR_SET(ac_cv_func_getopt_long, no)])])

AS_IF([test AS_VAR_GET(ac_cv_func_getopt_long) = yes],
      [AC_DEFINE_UNQUOTED(
        [USE_GETOPT_LONG], [1],
        [Define to 1 if you want to use getopt_long])],
      [])

dnl ###########################################################################
dnl Checks for library functions...
dnl ###########################################################################

# I don't think we actually do anything with these anymore.
AC_FUNC_MEMCMP
AC_FUNC_MMAP

# Most of these will be implemented by us if they are not defined by our
# current platform.
LIBGIFT_CHECK_FUNCS(ftruncate    \
                    gettimeofday \
                    madvise      \
                    memdup       \
                    mkstemp      \
                    nice         \
                    select       \
                    poll         \
                    rmdir        \
                    signal       \
                    socket       \
                    socketpair   \
                    snprintf     \
                    _snprintf    \
                    vsnprintf    \
                    _vsnprintf   \
                    strcasecmp   \
                    strcspn      \
                    strlcat      \
                    strlcpy      \
                    strmove      \
                    strpbrk      \
                    strspn       \
                    strtol       \
                    strtoul      \
                    unlink)

dnl ###########################################################################
dnl Handle the configure switches...
dnl ###########################################################################

AC_ARG_ENABLE(perl,        [  --enable-perl           enable perl support],[enable_perl=$enableval],[enable_perl=no])
AM_CONDITIONAL(PERL,        test x$enable_perl     = xyes)
AC_ARG_ENABLE(ltdl,        [  --disable-ltdl          do not use libltdl],[enable_ltdl=$enableval],[enable_ltdl=yes])
AM_CONDITIONAL(LTDL,        test x$enable_ltdl     = xyes)
AC_ARG_ENABLE(libvorbis,   [  --disable-libvorbis     do not use libvorbis],[use_libvorbis=$enableval],[use_libvorbis=yes])
AM_CONDITIONAL(LIBVORBIS,   test x$use_libvorbis   = xyes)
AC_ARG_ENABLE(imagemagick, [  --disable-imagemagick   do not use ImageMagick],[use_imagemagick=$enableval],[use_imagemagick=yes])
AM_CONDITIONAL(IMAGEMAGICK, test x$use_imagemagick = xyes)
AC_ARG_ENABLE(libmagic,    [  --enable-libmagic       use libmagic],[use_libmagic=$enableval],[use_libmagic=no])
AM_CONDITIONAL(LIBMAGIC,    test x$use_libmagic    = xyes)

dnl ###########################################################################
dnl Overrides...
dnl ###########################################################################

CFLAGS="$CFLAGS -Wall"

if test x$USE_MAINTAINER_MODE = xyes; then
  CFLAGS="$CFLAGS -g3 -O0"
fi

dnl ###########################################################################
dnl Setup some misc build environment settings...
dnl ###########################################################################

# this should be used by every build target
GIFT_CFLAGS="$CFLAGS"
GIFT_LIBS="$LIBS"
GIFT_LDFLAGS="$LDFLAGS"

AC_SUBST(GIFT_CFLAGS)
AC_SUBST(GIFT_LIBS)
AC_SUBST(GIFT_LDFLAGS)

if test x$prefix = xNONE; then
    prefix="/usr/local"
    AC_SUBST(prefix)
fi

libgiftincdir=${prefix}/include/libgift
AC_SUBST(libgiftincdir)

plugindir=${prefix}/lib/giFT
AC_SUBST(plugindir)

LIBGIFT_DEFINE_UNQUOTED(PLUGIN_DIR, "${prefix}/lib/giFT")

giftdatadir=${prefix}/share/giFT
AC_SUBST(giftdatadir)

if test x$WIN32 != xyes; then
    LIBGIFT_DEFINE_UNQUOTED(DATA_DIR, "${prefix}/share/giFT")
fi

giftperldir=${prefix}/bin
AC_SUBST(giftperldir)

dnl ###########################################################################
dnl Check for libltdl...
dnl ###########################################################################

use_ltdl=no                            # set the defaults
if test x$enable_ltdl = xyes; then
    dnl check for lt_dlopen and lt_dlsym in libltdl
    AC_CHECK_LIB(ltdl, lt_dlopen,
        [AC_CHECK_LIB(ltdl, lt_dlsym, found_libltdl=yes, found_libltdl=no )] ,)

    dnl check for ltdl header files
    AC_CHECK_HEADERS(ltdl.h,
            [ found_ltdlh=yes ],
            [ found_ltdlh=no ])

    # only add flags for ltdl stuff if all the tests passed
    if test x$found_libltdl = xyes -a x$found_ltdlh = xyes; then
        use_ltdl=yes

        DL_LIBS=-lltdl
	AC_SUBST(DL_LIBS)
	AC_SUBST(DL_LDFLAGS)
	AC_SUBST(DL_CFLAGS)

        AC_DEFINE(USE_LTDL)
        AC_SUBST(USE_LTDL)
    fi
fi

if test x$use_ltdl = xno; then
    AC_MSG_ERROR([
libltdl support is temporarily required.  Please install the appropriate
library and header files (which includes the -dev package).  See config.log
for more details.
                 ])
fi

dnl ###########################################################################
dnl Check for perl/libperl (deprecated)...
dnl ###########################################################################

enable_perl=no                         # force off for now
if test x$enable_perl = xyes ; then
    AC_PATH_PROG(perlpath, perl)
    AC_MSG_CHECKING(for Perl compile flags)
    PERL_CFLAGS=`$perlpath -MExtUtils::Embed -e ccopts 2>/dev/null`
    PERL_CFLAGS=`echo $PERL_CFLAGS | $SED 's/-D_FILE_OFFSET_BITS=[[^ ]]*//'`
    if test "_$PERL_CFLAGS" = _ ; then
        AC_MSG_RESULT([not found, building without perl.])
        enable_perl=no
    else
        PERL_LIBS=`$perlpath -MExtUtils::Embed -e ldopts 2>/dev/null | $SED 's/-lgdbm //'`
        PERL_LIBS=`echo $PERL_LIBS | $SED 's/-ldb //'`
        PERL_LIBS=`echo $PERL_LIBS | $SED 's/-lndbm //'`
        if test "$system" = "Linux"; then
            PERL_LIBS=`echo $PERL_LIBS | $SED 's/-lnsl //'`
            PERL_LIBS=`echo $PERL_LIBS | $SED 's/-lposix //'`
        fi
        PERL_LIBS=`echo $PERL_LIBS | $SED 's/-lc //'`
        AC_MSG_RESULT(ok)
        AC_CHECK_LIB(perl,perl_parse,[
        AC_SUBST(PERL_CFLAGS)
        AC_SUBST(PERL_LIBS)
        AC_DEFINE(USE_PERL)
        AC_CHECK_FUNCS(Perl_eval_pv)],enable_perl=no,$PERL_LIBS)
    fi
fi

dnl ###########################################################################
dnl Check for libvorbis...
dnl ###########################################################################

if test x$use_libvorbis = xyes
then
  GIFT_CHECK_VORBIS([AC_DEFINE(USE_LIBVORBIS)],
                    [use_libvorbis=no])
else
  use_libvorbis=no
fi

dnl ###########################################################################
dnl Check for ImageMagick...
dnl ###########################################################################

if test x$use_imagemagick = xyes
then
  OPT_MAGICK="/usr/local"
  AC_ARG_WITH(magick,dnl
    [
--with-Magick[=DIR]       where to look for ImageMagick [compiler/linker default paths]
                          DIR points to the ImageMagick installation prefix [/usr/local]
    ],
    OPT_MAGICK=$withval,
  )

  MAGICK_LDFLAGS="`Magick-config --ldflags 2>/dev/null`"
  MAGICK_LIBS="`Magick-config --libs 2>/dev/null`"
  save_LIBS="$LIBS"
  LIBS="$LIBS $MAGICK_LIBS"
  save_LDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS $MAGICK_LDFLAGS"
  AC_CHECK_LIB(Magick, ReadImage,
                [ IMAGEMAGICK_CFLAGS=''
                  IMAGEMAGICK_LDFLAGS="$MAGICK_LDFLAGS"
                  IMAGEMAGICK_LIBS="$MAGICK_LIBS"
                  AC_SUBST(IMAGEMAGICK_CFLAGS)
                  AC_SUBST(IMAGEMAGICK_LDFLAGS)
                  AC_SUBST(IMAGEMAGICK_LIBS)
                  AC_DEFINE(USE_IMAGEMAGICK) ],
               use_imagemagick=no)
  LIBS="$save_LIBS"
  LDFLAGS="$save_LDFLAGS"
else
  use_imagemagick=no
fi

dnl ###########################################################################
dnl Check for libmagic (think /usr/bin/file)...
dnl ###########################################################################

if test x$use_libmagic = xyes
then
  AC_CHECK_LIB(magic, magic_open)
  AC_CHECK_HEADERS(magic.h,
                   AC_DEFINE(USE_LIBMAGIC),
                   AC_ERROR(magic.h is required for libmagic support))
else
  use_libmagic=no
fi

dnl ###########################################################################
dnl Build the features for lib/giftconfig.h.in...
dnl ###########################################################################

LIBGIFT_DEFINE_UNQUOTED(GIFT_PACKAGE, "giFT")
LIBGIFT_DEFINE_UNQUOTED(GIFT_VERSION, "$GIFT_VERSION")

AC_DEFUN([LIBGIFT_HEADER],
         [AS_IF([test "${ac_cv_header_$1}" = yes],
                [LIBGIFT_DEFINE([$2])],
                [])
          ])

# TODO: Wrap AC_CHECK_HEADERS usage above.
# NOTE: C99's stdbool.h is handled separately
LIBGIFT_HEADER(db_h, HAVE_DB_H)
LIBGIFT_HEADER(db3_db_h, HAVE_DB3_DB_H)
LIBGIFT_HEADER(db4_db_h, HAVE_DB4_DB_H)
LIBGIFT_HEADER(dirent_h, HAVE_DIRENT_H)
LIBGIFT_HEADER(fcntl_h, HAVE_FCNTL_H)
LIBGIFT_HEADER(getopt_h, HAVE_GETOPT_H)
LIBGIFT_HEADER(inttypes_h, HAVE_INTTYPES_H)
LIBGIFT_HEADER(io_h, HAVE_IO_H)
LIBGIFT_HEADER(limits_h, HAVE_LIMITS_H)
LIBGIFT_HEADER(linux_limits_h, HAVE_LINUX_LIMITS_H)
LIBGIFT_HEADER(memory_h, HAVE_MEMORY_H)
LIBGIFT_HEADER(ndir_h, HAVE_NDIR_H)
LIBGIFT_HEADER(stdlib_h, HAVE_STDLIB_H)
LIBGIFT_HEADER(stdint_h, HAVE_STDINT_H)
LIBGIFT_HEADER(strings_h, HAVE_STRINGS_H)
LIBGIFT_HEADER(syslog_h, HAVE_SYSLOG_H)
LIBGIFT_HEADER(sys_dir_h, HAVE_SYS_DIR_H)
LIBGIFT_HEADER(sys_mman_h, HAVE_SYS_MMAN_H)
LIBGIFT_HEADER(sys_ndir_h, HAVE_SYS_NDIR_H)
LIBGIFT_HEADER(sys_stat_h, HAVE_SYS_STAT_H)
LIBGIFT_HEADER(sys_types_h, HAVE_SYS_TYPES_H)
LIBGIFT_HEADER(unistd_h, HAVE_UNISTD_H)

GIFT_FEATURES=gconfdefs.h
AC_SUBST_FILE(GIFT_FEATURES)

dnl ###########################################################################
dnl Finally do the generation...
dnl ###########################################################################

AC_CONFIG_FILES([
Makefile
gift.spec
gift-setup
data/Makefile
doc/Makefile
etc/Makefile
etc/ui/Makefile
lib/Makefile
lib/libgift.pc
lib/giftconfig.h
m4/Makefile
plugin/Makefile
plugin/protocol_ver.h
src/Makefile
src/meta/Makefile
win32/Makefile
win32/giFTsetup/Makefile
win32/giFTtray/Makefile
])

AC_CONFIG_COMMANDS([default],[[
chmod +x gift-setup
]],[[]])

AC_OUTPUT

dnl ###########################################################################
dnl Print summary to the user...
dnl ###########################################################################

echo
echo "$PACKAGE_NAME $VERSION"
echo
echo "-------------------- core ---"
echo
echo "giftd...................: yes"
echo "libgift.................: yes"
echo "libgiftproto............: yes"
echo "use ltdl................: $use_ltdl"
echo
echo "--------- meta data tools ---"
echo
echo "use libvorbis...........: $use_libvorbis"
echo "use ImageMagick.........: $use_imagemagick"
echo "use libmagic............: $use_libmagic"
echo
echo "-----------------------------"
echo
echo "giftd will be installed in $prefix/bin"
echo
echo "*** WARNING ***"
echo
echo "This distribution does not include a client or a network plugin!"
echo "Please see http://www.giftproject.org/ for more information."
echo
echo "*** WARNING ***"
echo
