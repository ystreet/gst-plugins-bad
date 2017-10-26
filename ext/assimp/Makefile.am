plugin_LTLIBRARIES = libgstassimp.la

libgstassimp_la_SOURCES = \
	gstassimp.c

noinst_HEADERS = \
	gstassimp.h

libgstassimp_la_CFLAGS = \
	-I$(top_srcdir)/gst-libs \
	-I$(top_builddir)/gst-libs \
	$(GST_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(ASSIMP_CFLAGS) \
	$(GRAPHENE_CFLAGS)

libgstassimp_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/3d/libgst3d-$(GST_API_VERSION).la \
	$(GST_BASE_LIBS) \
	$(GST_PLUGINS_BASE_LIBS) \
	-lgstvideo-$(GST_API_VERSION) \
	-lgstpbutils-$(GST_API_VERSION) \
	$(LIBM) \
	$(ASSIMP_LIBS) \
	$(GRAPHENE_LIBS)

libgstassimp_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstassimp_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)
