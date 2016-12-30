ORC_SOURCE=gst3dvertexpack
include $(top_srcdir)/common/orc.mak

lib_LTLIBRARIES = libgst3d-@GST_API_VERSION@.la

libgst3d_@GST_API_VERSION@_la_SOURCES = \
	gst3dmaterialinfo.c \
	gst3dmaterialmeta.c \
	gst3dmodelmeta.c \
	gst3dvertexconverter.c \
	gst3dvertexformat.c \
	gst3dvertexinfo.c \
	gst3dvertexmeta.c

nodist_libgst3d_@GST_API_VERSION@_la_SOURCES = \
	$(ORC_NODIST_SOURCES)

libgst3d_@GST_API_VERSION@includedir = $(includedir)/gstreamer-@GST_API_VERSION@/gst/3d
libgst3d_@GST_API_VERSION@include_HEADERS = \
	gst3dmaterialinfo.h \
	gst3dmaterialmeta.h \
	gst3dmodelmeta.h \
	gst3dvertexconverter.h \
	gst3dvertexformat.h \
	gst3dvertexinfo.h \
	gst3dvertexmeta.h \
	gst3dvertexpack.h

libgst3d_@GST_API_VERSION@_la_CFLAGS = \
	-I$(top_srcdir)/gst-libs \
	-I$(top_builddir)/gst-libs \
	$(GST_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(ORC_CFLAGS) \
	$(GRAPHENE_CFLAGS)

libgst3d_@GST_API_VERSION@_la_LIBADD = \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	$(ORC_LIBS) \
	$(GRAPHENE_LIBS)

libgst3d_@GST_API_VERSION@_la_LDFLAGS = \
	$(GST_LIB_LDFLAGS) \
	$(GST_ALL_LDFLAGS) \
	$(GST_LT_LDFLAGS)


