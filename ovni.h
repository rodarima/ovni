/*
 * MIT License
 * 
 * Copyright (c) 2021 Barcelona Supercomputing Center (BSC)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef OVNI_H
#define OVNI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <limits.h>

/* Hardcode the JSON_Value to avoid a dependency with janson */
typedef struct json_value_t  JSON_Value;

#define OVNI_TRACEDIR "ovni"
#define OVNI_MAX_HOSTNAME 512

/* Reserved buffer for event allocation per thread */
#define OVNI_MAX_EV_BUF (2 * 1024LL * 1024LL) /* 2 MiB */

/* ----------------------- common ------------------------ */

enum ovni_ev_flags {
	OVNI_EV_JUMBO = 0x10,
};

struct __attribute__((__packed__)) ovni_jumbo_payload {
	uint32_t size;
	uint8_t data[1];
};

union __attribute__((__packed__)) ovni_ev_payload {

	int8_t i8[16];
	int16_t i16[8];
	int32_t i32[4];
	int64_t i64[2];

	uint8_t u8[16];
	uint16_t u16[8];
	uint32_t u32[4];
	uint64_t u64[2];

	struct ovni_jumbo_payload jumbo;
};

struct __attribute__((__packed__)) ovni_ev_header {
	/* first 4 bits reserved, last 4 for payload size */
	uint8_t flags;
	uint8_t model;
	uint8_t category;
	uint8_t value;
	uint64_t clock;
};

struct __attribute__((__packed__)) ovni_ev {
	struct ovni_ev_header header;

	/* The payload size may vary depending on the ev type:
	 *   - normal: 0, or 2 to 16 bytes
	 *   - jumbo: 0 to 2^32 - 1 bytes */
	union ovni_ev_payload payload;
};

/* ----------------------- runtime ------------------------ */

/* State of each thread on runtime */
struct ovni_rthread {
	/* Current thread id */
	pid_t tid;

	/* Stream trace file descriptor */
	int streamfd;

	int ready;

	/* The number of bytes filled with events */
	size_t evlen;

	/* Buffer to write events */
	uint8_t *evbuf;
};

/* State of each process on runtime */
struct ovni_rproc {
	/* Path of the process tracedir */
	char dir[PATH_MAX];

	int app;
	int pid;
	char loom[OVNI_MAX_HOSTNAME];
	int ncpus;
	clockid_t clockid;
	char procdir[PATH_MAX];

	int ready;

	JSON_Value *meta;
};

void ovni_proc_init(int app, const char *loom, int pid);

/* Sets the MPI rank of the current process and the number of total nranks */
void ovni_proc_set_rank(int rank, int nranks);

void ovni_proc_fini(void);

void ovni_thread_init(pid_t tid);

void ovni_thread_free(void);

int ovni_thread_isready(void);

void ovni_ev_set_mcv(struct ovni_ev *ev, const char *mcv);

/* Gets the event clock in ns */
uint64_t ovni_ev_get_clock(const struct ovni_ev *ev);

/* Sets the event clock in ns */
void ovni_ev_set_clock(struct ovni_ev *ev, uint64_t clock);

/* Returns the current value of the ovni clock in ns */
uint64_t ovni_clock_now(void);

void ovni_payload_add(struct ovni_ev *ev, const uint8_t *buf, int size);

int ovni_ev_size(const struct ovni_ev *ev);

int ovni_payload_size(const struct ovni_ev *ev);

void ovni_add_cpu(int index, int phyid);

/* Adds the event to the events buffer. The buffer is flushed first if the event
 * doesn't fit in the buffer. */
void ovni_ev_emit(struct ovni_ev *ev);
void ovni_ev_jumbo_emit(struct ovni_ev *ev, const uint8_t *buf, uint32_t bufsize);

void ovni_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* OVNI_H */
