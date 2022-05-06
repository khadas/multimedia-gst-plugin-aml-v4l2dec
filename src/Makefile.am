plugin_LTLIBRARIES = libgstamlv4l2.la

# include $(top_srcdir)/common/gst-glib-gen.mak

libgstamlv4l2_la_SOURCES = gstamlv4l2.c \
				gstamlv4l2allocator.c \
				gstamlv4l2object.c \
				gstamlv4l2bufferpool.c \
				gstamlv4l2videodec.c \
				aml_v4l2_calls.c \
				aml-v4l2-utils.c

libgstamlv4l2_la_LIBADD =   $(GST_PLUGINS_BASE_LIBS) \
				 -lgstallocators-$(GST_API_VERSION) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_BASE_LIBS) \
				 $(GST_LIBS) \
				 $(LIBV4L2_LIBS) \
				 $(GUDEV_LIBS) \
				 $(LIBRT)
# compiler and linker flags used to compile this plugin, set in configure.ac
libgstamlv4l2_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstamlv4l2_la_CFLAGS = $(GST_CFLAGS) -Wno-error=format-security
# libgstvideo4linux2_la_LIBADD = $(GST_LIBS) -L$(TARGET_DIR)/usr/lib -L$(TARGET_DIR)/usr/lib/gstreamer-1.0 -lamlavsync
libgstamlv4l2_la_LIBADD = $(GST_LIBS) -L$(TARGET_DIR)/usr/lib -L$(TARGET_DIR)/usr/lib/gstreamer-1.0
libgstamlv4l2_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	ext/aml-vdec.h \
	ext/types-compat.h \
	ext/v4l2-common.h \
	ext/v4l2-controls.h \
	ext/videodev2.h \
	gstamlv4l2allocator.h \
	gstamlv4l2bufferpool.h \
	gstamlv4l2object.h \
	gstamlv4l2videodec.h \
	aml-v4l2-utils.h \
	gst/glib-compat-private.h