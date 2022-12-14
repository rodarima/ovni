/* Copyright (c) 2022 Barcelona Supercomputing Center (BSC)
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef INSTR_NOSV_H
#define INSTR_NOSV_H

#include "../instr.h"

static inline void
instr_nosv_type_create(int32_t typeid)
{
	struct ovni_ev ev = {0};

	ovni_ev_set_mcv(&ev, "VYc");
	ovni_ev_set_clock(&ev, ovni_clock_now());

	char buf[256];
	char *p = buf;

	size_t nbytes = 0;
	memcpy(buf, &typeid, sizeof(typeid));
	p += sizeof(typeid);
	nbytes += sizeof(typeid);
	sprintf(p, "testtype%d", typeid);
	nbytes += strlen(p) + 1;

	ovni_ev_jumbo_emit(&ev, (uint8_t *) buf, nbytes);
}

INSTR_2ARG(instr_nosv_task_create, "VTc", int32_t, id, uint32_t, typeid)
INSTR_1ARG(instr_nosv_task_execute, "VTx", int32_t, id)
INSTR_1ARG(instr_nosv_task_pause, "VTp", int32_t, id)
INSTR_1ARG(instr_nosv_task_resume, "VTr", int32_t, id)
INSTR_1ARG(instr_nosv_task_end, "VTe", int32_t, id)


#endif /* INSTR_NOSV_H */
