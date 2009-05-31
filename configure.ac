#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([cairo-dock], [2.0.4], [Fabrice Rey (fabounet@users.berlios.de)])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([foreign])
AC_PROG_LIBTOOL

IT_PROG_INTLTOOL()
AC_SUBST(ALL_LINGUAS)
AM_GLIB_GNU_GETTEXT
GETTEXT_PACKAGE=cairo-dock
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])
AC_SUBST(GETTEXT_PACKAGE)

CAIRO_DOCK_THEMES_DIR="themes"
AC_SUBST(CAIRO_DOCK_THEMES_DIR)

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([math.h stdlib.h])

# Checks for typedefs, structures, and compiler characteristics.
# AC_C_CONST
# AC_TYPE_PID_T

# Checks for library functions.
# AC_CHECK_FUNCS([sscanf])

PKG_CHECK_MODULES(PACKAGE, "gtk+-2.0 gthread-2.0 cairo librsvg-2.0 dbus-1 dbus-glib-1 libxml-2.0 gtkglext-1.0 xrender")

AC_ARG_ENABLE(xextend,
	[  --enable-xextend  :  Enable X Extensions],
	[case "${enableval}" in
		yes) usexextend=true ;;
		no)  usexextend=false ;;
		*) echo "bad value ${enableval} for --enable-xextend" ;;
	esac],[usexextend=true])

if test "$usexextend" = "true" ; then
	PKG_CHECK_MODULES(XEXTEND, "xtst xcomposite xinerama", [XExt_detected=true], [XExt_detected=false; AC_MSG_WARN(Could not find some X Extensions; Cairo-Dock won't be able to launch keyboard shortcuts, display window thumbnails, and use multiple screens.) ])
	AC_SUBST(XEXTEND_CFLAGS)
	AC_SUBST(XEXTEND_LIBS)
	
	if test ! "$XExt_detected" = "false" ; then
		AC_DEFINE(HAVE_XEXTEND,"1",[Defined if we can use X Extensions.])
	fi
fi
if test "$XExt_detected" = "true"; then
	xextend_required="xtst xcomposite xinerama"
else
	xextend_required=""
fi
AC_SUBST(xextend_required)

AM_CONDITIONAL( [WITH_XEXTEND], [test "$XExt_detected" = "true"] )

AC_ARG_ENABLE(glitz,
	[  --enable-glitz  :  Turn on hardware acceleration through Glitz (quite deprecated now)],
	[case "${enableval}" in
		yes) useglitz=true ;;
		no)  useglitz=false ;;
		*) echo "bad value ${enableval} for --enable-glitz" ;;
	esac],[useglitz=false])

if test "$useglitz" = "true" ; then
	PKG_CHECK_MODULES(GLITZ, glitz glitz-glx, [glitz_detected=true], [ glitz_detected=false; AC_MSG_WARN(Could not find glitz and/or glitz-glx; Cairo-Dock won't be built with Glitz hardware acceleration.) ])
	AC_SUBST(GLITZ_CFLAGS)
	AC_SUBST(GLITZ_LIBS)
	
	if test ! "$glitz_detected" = "false" ; then
		AC_DEFINE(HAVE_GLITZ,"1",[Defined if we can use glitz.])
	fi
fi

if test "$glitz_detected" = "true"; then
        glitz_required="glitz glitz-glx"
else
        glitz_required=""
fi
AC_SUBST(glitz_required)

AM_CONDITIONAL( [WITH_GLITZ], [test "$glitz_detected" = "true"] )

# We use crypt(3) which may be in libc, or in libcrypt (eg FreeBSD)
AC_CHECK_LIB(crypt, crypt, LIBCRYPT_LIBS="-lcrypt")
AM_CONDITIONAL( [HAVE_LIBCRYPT], [test "$LIBCRYPT_LIBS" != ""] )
AC_SUBST(LIBCRYPT_LIBS)

AC_CONFIG_FILES([Makefile
		cairo-dock.pc
		src/Makefile
		po/Makefile.in
		data/Makefile
		data/cairo-dock.conf
		data/main-dock.conf
		data/themes.conf
		data/default-theme/Makefile
		data/default-theme/launchers/Makefile
		data/default-theme/icons/Makefile
		data/default-theme/plug-ins/Makefile
		data/default-theme/plug-ins/Animated-icons/Makefile
		data/default-theme/plug-ins/Cairo-Penguin/Makefile
		data/default-theme/plug-ins/clock/Makefile
		data/default-theme/plug-ins/cpusage/Makefile
		data/default-theme/plug-ins/dialog-rendering/Makefile
		data/default-theme/plug-ins/dustbin/Makefile
		data/default-theme/plug-ins/GMenu/Makefile
		data/default-theme/plug-ins/icon-effect/Makefile
		data/default-theme/plug-ins/illusion/Makefile
		data/default-theme/plug-ins/logout/Makefile
		data/default-theme/plug-ins/rame/Makefile
		data/default-theme/plug-ins/rendering/Makefile
		data/default-theme/plug-ins/shortcuts/Makefile
		data/default-theme/plug-ins/showDesktop/Makefile
		data/default-theme/plug-ins/switcher/Makefile
		data/default-theme/plug-ins/weather/Makefile
		data/gauges/Makefile
		data/gauges/battery/Makefile
		data/gauges/turbo-night-fuel/Makefile
		data/emblems/Makefile
		data/explosion/Makefile])
AC_OUTPUT



echo "Cairo-Dock will be compiled with the following options :"
if test "$XExt_detected" = "true"; then
	echo " * Use X Extensions : yes"
else
	echo " * Use X Extensions : no"
fi
if test "$glitz_detected" = "true"; then
	echo " * Glitz Hardware Acceleration : yes"
else
	echo " * Glitz Hardware Acceleration : no"
fi
