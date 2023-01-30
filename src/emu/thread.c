/* Copyright (c) 2021-2023 Barcelona Supercomputing Center (BSC)
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "thread.h"

#include "path.h"
#include "bay.h"

static const char chan_fmt[] = "thread%lu.%s";
static const char *chan_name[] = {
	[TH_CHAN_CPU] = "cpu_gindex",
	[TH_CHAN_TID] = "tid_active",
	[TH_CHAN_STATE] = "state",
	[TH_CHAN_FLUSH] = "flush",
};

static const int chan_stack[] = {
	[TH_CHAN_FLUSH] = 1,
};

static const int chan_type[] = {
	[TH_CHAN_TID] = 2,
	[TH_CHAN_STATE] = 4,
	[TH_CHAN_CPU] = 6,
	[TH_CHAN_FLUSH] = 7,
};

static int
get_tid(const char *id, int *tid)
{
	/* The id must be like "loom.host01.123/proc.345/thread.567" */
	if (path_count(id, '/') != 2) {
		err("proc id can only contain two '/': %s", id);
		return -1;
	}

	/* Get the thread.567 part */
	const char *thname;
	if (path_strip(id, 2, &thname) != 0) {
		err("cannot get thread name");
		return -1;
	}

	/* Ensure the prefix is ok */
	const char prefix[] = "thread.";
	if (!path_has_prefix(thname, prefix)) {
		err("thread name must start with '%s': %s", prefix, thname);
		return -1;
	}

	/* Get the 567 part */
	const char *tidstr;
	if (path_next(thname, '.', &tidstr) != 0) {
		err("cannot find thread dot in '%s'", id);
		return -1;
	}

	*tid = atoi(tidstr);

	return 0;
}

int
thread_relpath_get_tid(const char *relpath, int *tid)
{
	return get_tid(relpath, tid);
}

int
thread_init_begin(struct thread *thread, struct proc *proc, const char *relpath)
{
	memset(thread, 0, sizeof(struct thread));

	thread->state = TH_ST_UNKNOWN;
	thread->gindex = -1;
	thread->proc = proc;

	if (snprintf(thread->id, PATH_MAX, "%s", relpath) >= PATH_MAX) {
		err("relpath too long");
		return -1;
	}

	if (get_tid(thread->id, &thread->tid) != 0) {
		err("cannot parse thread tid");
		return -1;
	}

	return 0;
}

void
thread_set_gindex(struct thread *th, int64_t gindex)
{
	th->gindex = gindex;
}

int
thread_init_end(struct thread *th)
{
	if (th->gindex < 0) {
		err("gindex not set");
		return -1;
	}

	for (int i = 0; i < TH_CHAN_MAX; i++) {
		enum chan_type type = CHAN_SINGLE;

		if (chan_stack[i])
			type = CHAN_STACK;

		chan_init(&th->chan[i], type,
				chan_fmt, th->gindex, chan_name[i]);
	}

	chan_prop_set(&th->chan[TH_CHAN_TID], CHAN_DUPLICATES, 1);

	th->is_init = 1;
	return 0;
}

int
thread_connect(struct thread *th, struct bay *bay, struct recorder *rec)
{
	if (!th->is_init) {
		err("thread is not initialized");
		return -1;
	}

	/* Get thread prv */
	struct pvt *pvt = recorder_find_pvt(rec, "thread");
	if (pvt == NULL) {
		err("cannot find thread pvt");
		return -1;
	}
	struct prv *prv = pvt_get_prv(pvt);

	for (int i = 0; i < TH_CHAN_MAX; i++) {
		struct chan *c = &th->chan[i];
		if (bay_register(bay, c) != 0) {
			err("bay_register failed");
			return -1;
		}

		long type = chan_type[i];
		long row = th->gindex;
		if (prv_register(prv, row, type, bay, c)) {
			err("prv_register failed");
			return -1;
		}
	}

	return 0;
}

int
thread_get_tid(struct thread *thread)
{
	return thread->tid;
}

/* Sets the state of the thread and updates the thread tracking channels */
int
thread_set_state(struct thread *th, enum thread_state state)
{
	/* The state must be updated when a cpu is set */
	if (th->cpu == NULL) {
		die("thread %d doesn't have a CPU", th->tid);
		return -1;
	}

	th->state = state;
	th->is_running = (state == TH_ST_RUNNING) ? 1 : 0;
	th->is_active = (state == TH_ST_RUNNING
					|| state == TH_ST_COOLING
					|| state == TH_ST_WARMING)
					? 1
					: 0;

	struct chan *st = &th->chan[TH_CHAN_STATE];
	if (chan_set(st, value_int64(th->state)) != 0) {
		err("thread_set_cpu: chan_set() failed");
		return -1;
	}

	struct value tid_active = value_null();

	if (th->is_active)
		tid_active = value_int64(th->tid);

	if (chan_set(&th->chan[TH_CHAN_TID], tid_active) != 0) {
		err("chan_set() failed");
		return -1;
	}

	return 0;
}

int
thread_select_active(struct mux *mux,
		struct value value,
		struct mux_input **input)
{
	if (value.type == VALUE_NULL) {
		*input = NULL;
		return 0;
	}

	if (value.type != VALUE_INT64) {
		err("expecting NULL or INT64 channel value");
		return -1;
	}

	enum thread_state state = (enum thread_state) value.i;

	if (mux->ninputs != 1) {
		err("expecting NULL or INT64 channel value");
		return -1;
	}

	switch (state) {
		case TH_ST_RUNNING:
		case TH_ST_COOLING:
		case TH_ST_WARMING:
			*input = mux->input;
			break;
		default:
			*input = NULL;
			break;
	}

	return 0;
}

int
thread_select_running(struct mux *mux,
		struct value value,
		struct mux_input **input)
{
	if (value.type == VALUE_NULL) {
		*input = NULL;
		return 0;
	}

	if (value.type != VALUE_INT64) {
		err("expecting NULL or INT64 channel value");
		return -1;
	}

	enum thread_state state = (enum thread_state) value.i;

	if (mux->ninputs != 1) {
		err("mux doesn't have one input but %d", mux->ninputs);
		return -1;
	}

	switch (state) {
		case TH_ST_RUNNING:
			*input = mux->input;
			break;
		default:
			*input = NULL;
			break;
	}

	return 0;
}



int
thread_set_cpu(struct thread *th, struct cpu *cpu)
{
	if (cpu == NULL) {
		err("thread_set_cpu: CPU is NULL\n");
		return -1;
	}

	if (th->cpu != NULL) {
		err("thread_set_cpu: thread %d already has a CPU\n", th->tid);
		return -1;
	}

	th->cpu = cpu;

	/* Update cpu channel */
	struct chan *c = &th->chan[TH_CHAN_CPU];
	if (chan_set(c, value_int64(cpu->gindex)) != 0) {
		err("thread_set_cpu: chan_set failed\n");
		return -1;
	}

	return 0;
}

int
thread_unset_cpu(struct thread *th)
{
	if (th->cpu == NULL) {
		err("thread_unset_cpu: thread %d doesn't have a CPU\n", th->tid);
		return -1;
	}

	th->cpu = NULL;

	struct chan *c = &th->chan[TH_CHAN_CPU];
	if (chan_set(c, value_null()) != 0) {
		err("thread_set_cpu: chan_set failed\n");
		return -1;
	}

	return 0;
}

int
thread_migrate_cpu(struct thread *th, struct cpu *cpu)
{
	if (th->cpu == NULL) {
		die("thread_migrate_cpu: thread %d doesn't have a CPU\n", th->tid);
		return -1;
	}

	th->cpu = cpu;

	struct chan *c = &th->chan[TH_CHAN_CPU];
	if (chan_set(c, value_int64(cpu->gindex)) != 0) {
		err("thread_set_cpu: chan_set failed\n");
		return -1;
	}

	return 0;
}