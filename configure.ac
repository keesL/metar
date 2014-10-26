#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
m4_include([VERSION.m4])dnl

AC_INIT([metar], [METAR_VERSION], [kees@leune.org])

AC_PREREQ(2.59)

AC_COPYRIGHT([
Copyright (C) 2004,2005 Kees Leune
])

AC_CONFIG_SRCDIR([src/metar.c])
AM_INIT_AUTOMAKE()

AC_PROG_CC
AC_HEADER_STDC
AC_TYPE_SIZE_T
AC_CHECK_LIB([curl], [curl_easy_init])
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])
AC_CHECK_FUNCS([memset regcomp])


if test "$ac_cv_lib_curl_curl_easy_init" = "no"
then
	AC_MSG_ERROR(Unable to find required library libcurl)
fi

AC_OUTPUT([ Makefile src/Makefile doc/Makefile ])
