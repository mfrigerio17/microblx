
AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS) -fvisibility=hidden

ubxmoddir = $(UBX_MODDIR)
ubxmod_LTLIBRARIES = pid.la

pid_la_SOURCES = pid.c
pid_la_LDFLAGS = -module -avoid-version -shared -export-dynamic
pid_la_LIBADD = $(top_builddir)/libubx/libubx.la

%.h.hexarr: %.h
	$(top_srcdir)/tools/ubx-tocarr -s $< -d $<.hexarr
