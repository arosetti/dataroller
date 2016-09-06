AC_PREREQ(2.61)
AC_INIT([dataroller], [0.1.4])
AC_PROG_CC
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall no-define foreign])

AC_CONFIG_HEADERS([config.h])

AC_CHECK_HEADERS([stdbool.h stdint.h stdlib.h unistd.h])
AC_CHECK_FUNCS(getopt)

#AC_CHECK_LIB(pthread, pthread_create)

#release
AC_ARG_ENABLE(
release,
[--enable-release=ARG enable release version (default=no)],
[enable_release=$enableval],
[enable_release=no]
)
if test "$enable_release" = "yes"; then
CFLAGS="-O2 -mtune=native -march=native -fomit-frame-pointer -floop-parallelize-all -ftree-parallelize-loops=4"
AC_DEFINE(NDEBUG,1,[disable asserts])
fi

#debug
AC_ARG_ENABLE(
debug,
[--enable-debug=ARG enable support for debugging (default=yes)],
[enable_debug=$enableval],
[enable_debug=yes]
)
if test "$enable_release" = "no"; then
if test "$enable_debug" = "yes"; then
CFLAGS="-O2 -g -Wall -Werror -pedantic"
AC_DEFINE(DEBUG,1,[debug option])
fi
fi

#profile
AC_ARG_ENABLE(
profile,
[ --enable-profile=ARG enable support for profiling (default=no) ],
[enable_profile=$enableval],
[enable_profile=no]
)
if test "$enable_profile" = "yes"; then
CFLAGS="$CFLAGS -pg"
fi

#likely
AC_ARG_ENABLE(
likely,
[ --enable-likely=ARG enable support for likely-unlikely (default=yes) ],
[enable_likely=$enableval],
[enable_likely=yes]
)
if test "$enable_likely" = "yes"; then
AC_DEFINE(USE_LIKELY,1,[likely option])
fi

#inline
AC_ARG_ENABLE(
inline,
[ --enable-inline=ARG enable inline functions (default=yes) ],
[enable_inline=$enableval],
[enable_inline=yes]
)
if test "$enable_inline" != "yes"; then
CFLAGS="$CFLAGS -fno-inline-functions"
else
AC_DEFINE(USE_INLINE,1,[mmap option])
fi

######## program options #########

#use_truncate_bit_encoding
AC_ARG_ENABLE(
inline,
[ --enable-truncate-bit-encoding=ARG use truncate bit encoding (default=yes) ],
[enable_truncate_bit_encoding=$enableval],
[enable_truncate_bit_encoding=yes]
)
if test "$enable_enable_truncate_bit_encoding" != "yes"; then
AC_DEFINE(USE_TRUNCATE_BIT_ENCODING,1,[truncate bit encoding option])
fi

CFLAGS="$CFLAGS -std=gnu99 -pipe"

DISTCLEANFILES="Makefile.in"
AC_SUBST(DISTCLEANFILES)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
