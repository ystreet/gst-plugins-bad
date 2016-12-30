plugin_LTLIBRARIES = libgst3d.la

libgst3d_la_SOURCES = \
	gst3d.c \
	gst3dvertexconvert.c \
	gst3dvertexsrc.c

noinst_HEADERS = \
	gst3dvertexconvert.h \
	gst3dvertexsrc.h

libgst3d_la_CFLAGS = \
	-I$(top_srcdir)/gst-libs \
	-I$(top_builddir)/gst-libs \
	$(GST_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GRAPHENE_CFLAGS)

libgst3d_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/3d/libgst3d-$(GST_API_VERSION).la \
	$(GST_BASE_LIBS) \
	$(GST_PLUGINS_BASE_LIBS) \
	$(GRAPHENE_LIBS)

libgst3d_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgst3d_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)
