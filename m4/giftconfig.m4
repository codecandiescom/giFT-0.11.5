###############################################################################
## $Id: giftconfig.m4,v 1.3 2003/07/28 05:09:33 jasta Exp $
###############################################################################

dnl #
dnl # GIFT_DEFINE(...)
dnl #
dnl # Wrapper for AC_DEFINE that helps readability when selecting the output
dnl # channel (./lib/giftconfig.h vs ./config.h).  This implementation uses
dnl # ./config.h.
dnl #

AC_DEFUN([GIFT_DEFINE],
         [AC_DEFINE($@)])

AC_DEFUN([GIFT_DEFINE_UNQUOTED],
         [AC_DEFINE_UNQUOTED($@)])

dnl #
dnl # LIBGIFT_DEFINE(...)
dnl #
dnl # Similar idea to AC_DEFINE, but used our own installed giftconfig.h
dnl # instead of the local one.  GLib does something similar with its
dnl # glibconfig.h, so don't think we're completely nuts :)
dnl #
dnl # Please note that we do not evaluate the third autoheader argument
dnl # because frankly we don't care.
dnl #

AC_DEFUN([LIBGIFT_DEFINE],
         [cat >>gconfdefs.h <<\_ACEOF
[@%:@ifndef] $1
[@%:@define] $1 m4_if($#, 2, [$2], $#, 3, [$2], 1)
[@%:@endif]
_ACEOF])

AC_DEFUN([LIBGIFT_DEFINE_UNQUOTED],
         [cat >>gconfdefs.h <<_ACEOF
[@%:@ifndef] $1
[@%:@define] $1 m4_if($#, 2, [$2], $#, 3, [$2], 1)
[@%:@endif]
_ACEOF])

AC_DEFUN([LIBGIFT_CLEAR_CONFIG],
         [rm -f gconfdefs.h])

###############################################################################

AC_DEFUN([LIBGIFT_CHECK_TYPE],
         [AC_CHECK_TYPE([$1],,
          [LIBGIFT_DEFINE_UNQUOTED([$1], [$2], [$3])],
          [$4])])

AC_DEFUN([LIBGIFT_CHECK_INTTYPE],
         [LIBGIFT_CHECK_TYPE([$1], [$2],
                             [Defined to '$2' if <sys/types.h> and <stdint.h> do not define.],
                             [#include <sys/types.h>
                              #ifdef HAVE_STDINT_H
                              #include <stdint.h>
                              #endif
                              #ifdef HAVE_INTTYPES_H
                              #include <inttypes.h>
                              #endif
                              ])])

AC_DEFUN([LIBGIFT_CHECK_FUNC],
         [AC_CHECK_FUNC([$1],
                        [LIBGIFT_DEFINE_UNQUOTED([AS_TR_CPP([HAVE_$1])])])])

AC_DEFUN([LIBGIFT_CHECK_FUNCS],
         [for ac_func in $1
          do
            LIBGIFT_CHECK_FUNC($ac_func)
          done
          ])
