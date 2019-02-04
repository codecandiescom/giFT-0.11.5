dnl
dnl $Id: winsock.m4,v 1.3 2003/07/28 05:09:33 jasta Exp $
dnl
dnl Check if the a library is necessary for linking a function.
dnl
dnl This could just use AC_CHECK_LIB, but that doesnt
dnl work with -lws2_32 or -lwsock32. I think this is
dnl because win32 api functions have mangled names, so
dnl AC_CHECK_LIB looks for "shutdown" when it needs to look
dnl for "shutdown@8" ('8' for two 4-byte parameters).
dnl
dnl Takes 4 args: shell variable to set, includes, function,
dnl               and library to test
dnl
AC_DEFUN([GIFT_NEEDS_LIB],
[dnl
    # Check for -lws2_32 lib
    AC_CACHE_CHECK([if -l$4 is needed], [$1],
    [dnl
        $1=no

        AC_TRY_LINK([$2], [$3],
          gift_cv_link_without_$1_failed=no,
          gift_cv_link_without_$1_failed=yes)

        if test x$gift_cv_link_without_$1_failed = xyes; then
            gift_cv_$1_save_LIBS="$LIBS"
            LIBS="$LIBS -l$4"

            AC_TRY_LINK([$2], [$3], $1=yes, $1=no)

            LIBS="$gift_cv_$1_save_LIBS"
        fi
    ])
])
dnl
dnl Check if -lws2_32 [winsock2] or -lwsock32 [winsock1]
dnl is necessary for linking to socket functions.
dnl
AC_DEFUN([GIFT_CHECK_WINSOCK_LIBS],
[dnl
    GIFT_NEEDS_LIB(gift_cv_need_ws2_lib,
                   [#include <winsock2.h>],
                   [shutdown ((void *) 0, 0)],
                   ws2_32)

    if test x$gift_cv_need_ws2_lib = xyes; then
        LIBS="$LIBS -lws2_32"
    else
        GIFT_NEEDS_LIB(gift_cv_need_wsock32_lib,
                       [#include <winsock2.h>],
                       [shutdown ((void *) 0, 0)],
                       wsock32)

        if test x$gift_cv_need_wsock32_lib = xyes; then
            LIBS="$LIBS -lwsock32"
        fi
    fi
])
