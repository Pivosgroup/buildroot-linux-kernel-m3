/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if you have the `strlcpy' function. */
/* #undef HAVE_STRLCPY */

/* Name of package */
#define PACKAGE "libnrx"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "support@nanoradio.com"

/* Define to the full name of this package. */
#define PACKAGE_NAME "libnrx"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libnrx 0.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libnrx"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.0"

/* Version number of package */
#define VERSION "0.0"

#ifndef HAVE_STRLCPY
size_t _nrx_strlcpy (char*, const char*, size_t);
#define strlcpy _nrx_strlcpy
#endif
