PHP_ARG_ENABLE(spiler, whether to enable spiler support,
[  --enable-spiler           Enable spiler support])

if test "$PHP_SPILER" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-spiler -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/spiler.h"  # you most likely want to change this
  dnl if test -r $PHP_SPILER/$SEARCH_FOR; then # path given as parameter
  dnl   SPILER_DIR=$PHP_SPILER
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for spiler files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       SPILER_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$SPILER_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the spiler distribution])
  dnl fi

  dnl # --with-spiler -> add include path
  dnl PHP_ADD_INCLUDE($SPILER_DIR/include)

  dnl # --with-spiler -> check for lib and symbol presence
  dnl LIBNAME=spiler # you may want to change this
  dnl LIBSYMBOL=spiler # you most likely want to change this

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $SPILER_DIR/$PHP_LIBDIR, SPILER_SHARED_LIBADD)
  AC_DEFINE(HAVE_SPILERLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong spiler lib version or lib not found])
  dnl ],[
  dnl   -L$SPILER_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(SPILER_SHARED_LIBADD)

  PHP_NEW_EXTENSION(spiler, php_spiler.c spiler_profiler.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
