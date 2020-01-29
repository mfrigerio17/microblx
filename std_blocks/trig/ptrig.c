/*
 * A pthread based trigger block
 */

/* #define DEBUG 1 */
#define CONFIG_PTHREAD_SETNAME

#ifdef CONFIG_PTHREAD_SETNAME
 #define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <pthread.h>
#include <limits.h>	/* PTHREAD_STACK_MIN */

#include "ubx.h"
#include "trig_utils.h"

#include "types/ptrig_period.h"
#include "types/ptrig_period.h.hexarr"

/* ptrig metadata */
char ptrig_meta[] =
	"{ doc='pthread based trigger',"
	"  realtime=true,"
	"}";

ubx_port_t ptrig_ports[] = {
	{ .name = "tstats", .out_type_name = "struct ubx_tstat", .doc = "out port for totals and per block timing statistics" },
	{ .name = "shutdown", .in_type_name = "int", .doc = "input port for stopping ptrig" },
	{ NULL },
};

/* types defined by ptrig block */
ubx_type_t ptrig_types[] = {
	def_struct_type(struct ptrig_period, &ptrig_period_h),
	{ NULL },
};

/* configuration */
ubx_config_t ptrig_config[] = {
	{ .name = "period", .type_name = "struct ptrig_period", .doc = "trigger period in { sec, ns }", },
	{ .name = "stacksize", .type_name = "size_t", .doc = "stacksize as per pthread_attr_setstacksize(3)" },
	{ .name = "sched_priority", .type_name = "int", .doc = "pthread priority" },
	{ .name = "sched_policy", .type_name = "char", .doc = "pthread scheduling policy" },
	{ .name = "thread_name", .type_name = "char", .doc = "thread name (for dbg), default is block name" },
	{ .name = "trig_blocks", .type_name = "struct ubx_trig_spec", .doc = "specification of blocks to trigger" },
	{ .name = "tstats_enabled", .type_name = "int", .doc = "enable timing statistics over all blocks", },
	{ .name = "tstats_profile_path", .type_name = "char", .doc = "file to which to write the timing statistics" },
	{ .name = "tstats_output_rate", .type_name = "unsigned int", .doc = "output tstats only on every tstats_output_rate'th trigger (0 to disable)" },
	{ NULL },
};


const char *tstat_global_id = "##total##";

/* instance state */
struct ptrig_inf {
	pthread_t tid;
	pthread_attr_t attr;
	uint32_t state;

	pthread_mutex_t mutex;
	pthread_cond_t active_cond;

	const struct ubx_trig_spec *trig_list;
	unsigned int trig_list_len;

	struct ptrig_period *period;

	/* timing statistics */
	int tstats_enabled;
	const char *tstats_profile_path;

	struct ubx_tstat global_tstats;
	struct ubx_tstat *blk_tstats;

	ubx_port_t *p_tstats;
};

def_write_fun(write_tstat, struct ubx_tstat)

/**
 * print tstats
 */
void ptrig_tstats_print(struct ptrig_inf *inf)
{
	unsigned int i;

	if (inf->tstats_enabled) {
		for (i = 0; i < inf->trig_list_len; i++)
			if (inf->trig_list[i].measure)
				tstat_print(inf->tstats_profile_path,
					    &inf->blk_tstats[i]);
	}
}

/* trigger the configured blocks */
static int trigger_steps(struct ptrig_inf *inf)
{
	int ret = -1;
	unsigned int i, steps;

	struct ubx_timespec ts_start, ts_end, blk_ts_start, blk_ts_end;

	if (inf->tstats_enabled)
		ubx_gettime(&ts_start);

	for (i = 0; i < inf->trig_list_len; i++) {
		if (inf->trig_list[i].measure)
			ubx_gettime(&blk_ts_start);

		for (steps = 0; steps < inf->trig_list[i].num_steps; steps++) {
			if (ubx_cblock_step(inf->trig_list[i].b) != 0)
				goto out;

			/* per block statistics */
			if (inf->trig_list[i].measure) {
				ubx_gettime(&blk_ts_end);
				tstat_update(&inf->blk_tstats[i],
					     &blk_ts_start,
					     &blk_ts_end);
			}

		}
	}

	/* compute global tstats */
	if (inf->tstats_enabled) {
		ubx_gettime(&ts_end);
		tstat_update(&inf->global_tstats, &ts_start, &ts_end);

		/* output global tstats - TODO throttling */
		write_tstat(inf->p_tstats, &inf->global_tstats);
	}

	/* output stats - TODO throttling */
	for (i = 0; i < inf->trig_list_len; i++) {
		if (inf->trig_list[i].measure)
			write_tstat(inf->p_tstats, &inf->blk_tstats[i]);
	}

	ret = 0;
 out:
	return ret;
}

/* helper for normalizing struct timespecs */
static inline void tsnorm(struct timespec *ts)
{
	if (ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_sec += ts->tv_nsec / NSEC_PER_SEC;
		ts->tv_nsec = ts->tv_nsec % NSEC_PER_SEC;
	}
}

/* thread entry */
static void *thread_startup(void *arg)
{
	int ret = -1;
	ubx_block_t *b;
	struct ptrig_inf *inf;
	struct timespec ts;

	b = (ubx_block_t *) arg;
	inf = (struct ptrig_inf *)b->private_data;

	while (1) {
		pthread_mutex_lock(&inf->mutex);

		while (inf->state != BLOCK_STATE_ACTIVE) {
			ptrig_tstats_print(inf);
			pthread_cond_wait(&inf->active_cond, &inf->mutex);
		}
		pthread_mutex_unlock(&inf->mutex);

		ret = clock_gettime(CLOCK_MONOTONIC, &ts);
		if (ret) {
			ubx_err(b, "clock_gettime failed: %s",
				strerror(errno));
			goto out;
		}

		trigger_steps(inf);

		ts.tv_sec += inf->period->sec;
		ts.tv_nsec += inf->period->usec * NSEC_PER_USEC;
		tsnorm(&ts);

		ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts,
				      NULL);
		if (ret) {
			ubx_err(b, "clock_nanosleep failed: %s",
				strerror(errno));
			goto out;
		}
	}

 out:
	/* block on cond var that signals block is running */
	pthread_exit(NULL);
}

int ptrig_handle_config(ubx_block_t *b)
{
	int ret = -1;
	unsigned int schedpol;
	long len;
	const char *schedpol_str;
	size_t *stacksize;
	const int *prio;
	struct sched_param sched_param; /* prio */
	struct ptrig_inf *inf = (struct ptrig_inf *)b->private_data;

	/* period */
	len = ubx_config_get_data_ptr(b, "period", (void **) &inf->period);
	if (len <= 0) {
		ubx_err(b, "%s: config 'period' unconfigured", b->name);
		goto out;
	}

	/* stacksize */
	len = ubx_config_get_data_ptr(b, "stacksize", (void **) &stacksize);
	if (len < 0)
		goto out;

	if (len > 0) {
		if (*stacksize < PTHREAD_STACK_MIN) {
			ubx_err(b, "%s: stacksize (%zd) less than PTHREAD_STACK_MIN (%d)",
				b->name, *stacksize, PTHREAD_STACK_MIN);
			goto out;
		}

		if (pthread_attr_setstacksize(&inf->attr, *stacksize)) {
			ubx_err(b, "pthread_attr_setstacksize failed: %s",
				strerror(errno));
			goto out;
		}
	}

	/* schedpolicy */
	len = cfg_getptr_char(b, "sched_policy", &schedpol_str);
	if (len < 0)
		goto out;

	if (len > 0) {
		if (strncmp(schedpol_str, "SCHED_OTHER", len) == 0) {
			schedpol = SCHED_OTHER;
		} else if (strncmp(schedpol_str, "SCHED_FIFO", len) == 0) {
			schedpol = SCHED_FIFO;
		} else if (strncmp(schedpol_str, "SCHED_RR", len) == 0) {
			schedpol = SCHED_RR;
		} else {
			ubx_err(b, "%s: sched_policy config: illegal value %s",
			    b->name, schedpol_str);
			goto out;
		}
	} else {
		schedpol = SCHED_OTHER;
	}

	if (pthread_attr_setschedpolicy(&inf->attr, schedpol))
		ubx_err(b, "pthread_attr_setschedpolicy failed");

	/* see PTHREAD_ATTR_SETSCHEDPOLICY(3) */
	ret = pthread_attr_setinheritsched(&inf->attr, PTHREAD_EXPLICIT_SCHED);

	if (ret != 0)
		ubx_err(b, "failed to set PTHREAD_EXPLICIT_SCHED: %s",
			strerror(errno));

	/* priority */
	len = cfg_getptr_int(b, "sched_priority", &prio);
	if (len < 0)
		goto out;

	sched_param.sched_priority = (len > 0) ? *prio : 0;

	if (((schedpol == SCHED_FIFO ||
	      schedpol == SCHED_RR) && sched_param.sched_priority == 0) ||
	     (schedpol == SCHED_OTHER && sched_param.sched_priority > 0)) {
		ubx_err(b, "%s sched_priority is %d with %s policy",
			b->name, sched_param.sched_priority, schedpol_str);
	}

	ret = pthread_attr_setschedparam(&inf->attr, &sched_param);
	if (ret != 0) {
		ubx_err(b, "failed to set sched_policy.sched_priority to %d: %s",
			sched_param.sched_priority, strerror(ret));
		ret = EINVALID_CONFIG;
		goto out;
	}

	ubx_debug(b, "config: period=%lus:%luus, policy=%s, prio=%d, stacksize=%lu (0=default size)",
		  inf->period->sec, inf->period->usec,
		  schedpol_str, sched_param.sched_priority,
		  (unsigned long) stacksize);

	ret = 0;
out:
	return ret;
}

/* init */
static int ptrig_init(ubx_block_t *b)
{
	long len;
	int ret = EOUTOFMEM;
	const char *threadname;
	struct ptrig_inf *inf;

	b->private_data = calloc(1, sizeof(struct ptrig_inf));
	if (b->private_data == NULL) {
		ubx_err(b, "failed to alloc");
		goto out;
	}

	inf = (struct ptrig_inf *)b->private_data;

	inf->state = BLOCK_STATE_INACTIVE;

	pthread_cond_init(&inf->active_cond, NULL);
	pthread_mutex_init(&inf->mutex, NULL);
	pthread_attr_init(&inf->attr);
	pthread_attr_setdetachstate(&inf->attr, PTHREAD_CREATE_JOINABLE);

	if (ptrig_handle_config(b) != 0) {
		ret = EINVALID_CONFIG;
		goto out_err;
	}

	ret = pthread_create(&inf->tid, &inf->attr, thread_startup, b);
	if (ret != 0) {
		ubx_err(b, "pthread_create failed: %s", strerror(errno));
		goto out_err;
	}

#ifdef CONFIG_PTHREAD_SETNAME
	len = cfg_getptr_char(b, "thread_name", &threadname);
	if (len < 0)
		goto out_err;

	threadname = (len > 0) ? threadname : b->name;

	if (pthread_setname_np(inf->tid, threadname))
		ubx_err(b, "failed to set thread_name to %s", threadname);
#endif

	/* OK */
	ret = 0;
	goto out;

 out_err:
	free(b->private_data);
 out:
	return ret;
}

static int ptrig_start(ubx_block_t *b)
{
	int ret = -1;
	const int *val;
	long len;
	struct ptrig_inf *inf;
	FILE *fp;

	inf = (struct ptrig_inf *)b->private_data;

	len = ubx_config_get_data_ptr(b, "trig_blocks", (void **)&inf->trig_list);
	if (len < 0)
		goto out;

	inf->trig_list_len = len;

	/* preparing timing statistics */
	tstat_init(&inf->global_tstats, tstat_global_id);

	/* this is freed in cleanup hook */
	inf->blk_tstats = realloc(
		inf->blk_tstats,
		inf->trig_list_len * sizeof(struct ubx_tstat));

	if (!inf->blk_tstats) {
		ubx_err(b, "failed to alloc blk_stats");
		goto out;
	}

	for (unsigned int i = 0; i < inf->trig_list_len; i++) {
		tstat_init(&inf->blk_tstats[i],
			   inf->trig_list[i].b->name);
	}

	len = cfg_getptr_char(b, "tstats_profile_path", &inf->tstats_profile_path);
	if (len < 0) {
		ubx_err(b, "unable to retrieve tstats_profile_path parameter");
		goto out;
	}
	/* truncate the file if it exists */
	if (len > 0) {
		fp = fopen(inf->tstats_profile_path, "w");
		if (fp)
			fclose(fp);
	}

	len = cfg_getptr_int(b, "tstats_enabled", &val);
	if (len < 0)
		goto out;

	inf->tstats_enabled = (len > 0) ? *val : 0;

	if (inf->tstats_enabled)
		ubx_info(b, "tstats enabled");

	pthread_mutex_lock(&inf->mutex);
	inf->state = BLOCK_STATE_ACTIVE;
	pthread_cond_signal(&inf->active_cond);
	pthread_mutex_unlock(&inf->mutex);

	inf->p_tstats = ubx_port_get(b, "tstats");
	ret = 0;

out:
	return ret;
}

static void ptrig_stop(ubx_block_t *b)
{
	struct ptrig_inf *inf = (struct ptrig_inf *)b->private_data;

	pthread_mutex_lock(&inf->mutex);
	inf->state = BLOCK_STATE_INACTIVE;
	pthread_mutex_unlock(&inf->mutex);
}

static void ptrig_cleanup(ubx_block_t *b)
{
	int ret;
	struct ptrig_inf *inf = (struct ptrig_inf *)b->private_data;

	inf->state = BLOCK_STATE_PREINIT;

	ret = pthread_cancel(inf->tid);
	if (ret != 0)
		ubx_err(b, "pthread_cancel failed: %s", strerror(errno));

	/* join */
	ret = pthread_join(inf->tid, NULL);
	if (ret != 0)
		ubx_err(b, "pthread_join failed: %s", strerror(errno));

	pthread_attr_destroy(&inf->attr);

	free(inf->blk_tstats);
	free(b->private_data);
}

/* put everything together */
ubx_block_t ptrig_comp = {
	.name = "std_triggers/ptrig",
	.type = BLOCK_TYPE_COMPUTATION,
	.attrs = BLOCK_ATTR_TRIGGER | BLOCK_ATTR_ACTIVE,
	.meta_data = ptrig_meta,

	.configs = ptrig_config,
	.ports = ptrig_ports,

	.init = ptrig_init,
	.start = ptrig_start,
	.stop = ptrig_stop,
	.cleanup = ptrig_cleanup
};

static int ptrig_mod_init(ubx_node_info_t *ni)
{
	int ret;
	ubx_type_t *tptr;

	for (tptr = ptrig_types; tptr->name != NULL; tptr++) {
		ret = ubx_type_register(ni, tptr);
		if (ret != 0) {
			ubx_log(UBX_LOGLEVEL_ERR, ni,
				__func__,
				"failed to register type %s", tptr->name);
			goto out;
		}
	}
	ret = ubx_block_register(ni, &ptrig_comp);
	if (ret != 0) {
		ubx_log(UBX_LOGLEVEL_ERR, ni, __func__,
			"failed to register ptrig block");
	}
 out:
	return ret;
}

static void ptrig_mod_cleanup(ubx_node_info_t *ni)
{
	ubx_type_t *tptr;

	for (tptr = ptrig_types; tptr->name != NULL; tptr++)
		ubx_type_unregister(ni, tptr->name);

	ubx_block_unregister(ni, "std_triggers/ptrig");
}

UBX_MODULE_INIT(ptrig_mod_init)
UBX_MODULE_CLEANUP(ptrig_mod_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
