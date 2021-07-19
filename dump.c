#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdatomic.h>

#include "def.h"

int load_proc(struct ovniproc *proc, char *procdir)
{
	FILE *f;
	int cpu;
	char path[PATH_MAX];

	for(cpu=0; cpu<MAX_CPU; cpu++)
	{
		sprintf(path, "%s/cpu.%d", procdir, cpu);
		f = fopen(path, "r");

		if(f == NULL)
			break;

		proc->cpustream[cpu] = f;
	}

	proc->ncpus = cpu;

	return 0;
}

int load_loom(struct ovniloom *loom, char *loomdir)
{
	int proc;
	char path[PATH_MAX];
	struct stat st;

	for(proc=0; proc<MAX_PROC; proc++)
	{
		sprintf(path, "%s/proc.%d", loomdir, proc);

		if(stat(path, &st) != 0)
		{
			/* Proc numbers exausted */
			if(errno == ENOENT)
				break;

			fprintf(stderr, "cannot stat %s: %s\n", path,
					strerror(errno));
			return -1;
		}

		if(!S_ISDIR(st.st_mode))
		{
			fprintf(stderr, "not a dir %s\n", path);
			return -1;
		}

		if(load_proc(&loom->proc[proc], path))
			return -1;
	}

	loom->nprocs = proc;

	return 0;
}

int load_trace(struct ovnitrace *trace, char *tracedir)
{
	int loom, nlooms;
	char path[PATH_MAX];

	/* TODO: For now only one loom */
	nlooms = 1;
	loom = 0;

	sprintf(path, "%s/loom.%d", tracedir, loom);

	if(load_loom(&trace->loom[loom], path))
		return -1;

	trace->nlooms = nlooms;

	return 0;
}

/* Populates the streams in a single array */
int load_streams(struct ovnitrace *trace)
{
	int i, j, k, s;
	struct ovniloom *loom;
	struct ovniproc *proc;
	struct ovnistream *stream;

	trace->nstreams = 0;

	for(i=0; i<trace->nlooms; i++)
	{
		loom = &trace->loom[i];
		for(j=0; j<loom->nprocs; j++)
		{
			proc = &loom->proc[j];
			for(k=0; k<proc->ncpus; k++)
			{
				trace->nstreams++;
			}
		}
	}

	trace->stream = calloc(trace->nstreams, sizeof(struct ovnistream));

	if(trace->stream == NULL)
	{
		perror("calloc");
		return -1;
	}

	fprintf(stderr, "loaded %d streams\n", trace->nstreams);

	for(s=0, i=0; i<trace->nlooms; i++)
	{
		loom = &trace->loom[i];
		for(j=0; j<loom->nprocs; j++)
		{
			proc = &loom->proc[j];
			for(k=0; k<proc->ncpus; k++)
			{
				stream = &trace->stream[s++];
				stream->f = proc->cpustream[k];
				stream->cpu = k;
				stream->active = 1;
			}
		}
	}

	return 0;
}

int load_first_event(struct ovnitrace *trace)
{
	int i;
	struct ovnistream *stream;
	struct ovnievent *ev;

	for(i=0; i<trace->nstreams; i++)
	{
		stream = &trace->stream[i];
		ev = &stream->last;
		if(fread(ev, sizeof(*ev), 1, stream->f) != 1)
		{
			fprintf(stderr, "failed to read and event\n");
			return -1;
		}

		fprintf(stderr, "ev clock %u\n", ev->clock);
	}

	return 0;
}

void emit(struct ovnievent *ev, int cpu)
{
	static uint64_t lastclock = 0;
	uint64_t delta;

	delta = ev->clock - lastclock;

	printf("%d %c %c %d %020lu (+%lu) ns\n",
			cpu, ev->fsm, ev->event, ev->data, ev->clock, delta);

	lastclock = ev->clock;
}

void load_next_event(struct ovnistream *stream)
{
	int i;
	size_t n;
	struct ovnievent *ev;

	if(!stream->active)
		return;

	ev = &stream->last;
	if((n = fread(ev, sizeof(*ev), 1, stream->f)) != 1)
	{
		//fprintf(stderr, "failed to read an event, n=%ld\n", n);
		stream->active = 0;
		return;
	}

	stream->active = 1;
}

void dump_events(struct ovnitrace *trace)
{
	int i, f;
	uint64_t minclock, lastclock;
	struct ovnievent *ev;
	struct ovnistream *stream;

	/* Load events */
	for(i=0; i<trace->nstreams; i++)
	{
		stream = &trace->stream[i];
		load_next_event(stream);
	}

	lastclock = 0;

	while(1)
	{
		f = -1;
		minclock = 0;

		/* Select next event based on the clock */
		for(i=0; i<trace->nstreams; i++)
		{
			stream = &trace->stream[i];

			if(!stream->active)
				continue;

			ev = &stream->last;
			if(f < 0 || ev->clock < minclock)
			{
				f = i;
				minclock = ev->clock;
			}
		}

		//fprintf(stderr, "f=%d minclock=%u\n", f, minclock);

		if(f < 0)
			break;

		stream = &trace->stream[f];

		if(lastclock >= stream->last.clock)
		{
			fprintf(stderr, "warning: backwards jump in time\n");
		}

		/* Emit current event */
		emit(&stream->last, stream->cpu);

		lastclock = stream->last.clock;

		/* Read the next one */
		load_next_event(stream);

		/* Unset the index */
		f = -1;
		minclock = 0;

	}
}

void free_streams(struct ovnitrace *trace)
{
	free(trace->stream);
}

int main(int argc, char *argv[])
{
	char *tracedir;
	struct ovnitrace trace;

	if(argc != 2)
	{
		fprintf(stderr, "missing tracedir\n");
		exit(EXIT_FAILURE);
	}

	tracedir = argv[1];

	if(load_trace(&trace, tracedir))
		return 1;

	if(load_streams(&trace))
		return 1;

	dump_events(&trace);

	free_streams(&trace);

	return 0;
}
