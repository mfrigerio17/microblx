AM_CFLAGS = -I$(top_srcdir)/libubx $(UBX_CFLAGS)

bin_PROGRAMS = ubx-log

ubx_log_SOURCES = $(top_srcdir)/libubx/ubx.h ubx-log.c
ubx_log_LDADD = $(top_builddir)/libubx/librtlog_client.la

dist_bin_SCRIPTS = ubx-tocarr \
		   ubx-genblock \
		   ubx-launch \
		   ubx-ilaunch \
		   ubx-modinfo \
		   ubx-mq
