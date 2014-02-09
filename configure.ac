AC_INIT([hidmap], [1.0], [bugs@beralt.nl])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_PROG_CC
AC_LANG_C

AC_PROG_MAKE_SET

AC_HEADER_STDC

PKG_CHECK_MODULES([LIBUSB], [libusb-1.0 >= 1.0.0])
PKG_CHECK_MODULES([DBUS], [dbus-1 >= 1.0.0])

oldCPPFLAGS="$CPPFLAGS"
RPI_VC_CPPFLAGS="-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -DUSE_VCHIQ_ARM"
CPPFLAGS="$CPPFLAGS $RPI_VC_CPPFLAGS"
AC_CHECK_HEADER([bcm_host.h], [], [AC_MSG_WARN([Broadcom VideoCore headers not found in /opt/vc/include])])
AC_SUBST(RPI_VC_CPPFLAGS, $RPI_VC_CPPFLAGS)
CPPFLAGS="$oldCPPFLAGS"

oldLDFLAGS="$LDFLAGS"
RPI_VC_LIBS="-L/opt/vc/lib"
LDFLAGS="$LDFLAGS $RPI_VC_LIBS"
AC_CHECK_LIB([bcm_host], [bcm_host_init], [], [AC_MSG_WARN([Broadcom VideoCore libraries not found in /opt/vc/lib])])
AC_SUBST(RPI_VC_LIBS, $RPI_VC_LIBS)
LDFLAGS="$oldLDFLAGS"

VERSION="1.0"
AC_SUBST(VERSION)
AC_SUBST(LIBUSB_1_0, $LIBUSB_1_0)
AC_SUBST(DBUS_1, $DBUS_1)
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

AC_OUTPUT
