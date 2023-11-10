/* Copyright (c) 2021-2023 Barcelona Supercomputing Center (BSC)
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdint.h>
#include "instr.h"
#include "instr_nosv.h"

int
main(void)
{
	instr_start(0, 1);
	instr_nosv_init();

	uint32_t typeid = 666;
	instr_nosv_type_create(typeid);

	uint32_t taskid = 1;
	instr_nosv_task_create(taskid, typeid);
	instr_nosv_task_execute(taskid);
	/* Change subsystem to prevent duplicates */
	instr_nosv_submit_enter();
	/* Run another nested task with same id (should fail) */
	instr_nosv_task_execute(taskid);

	instr_end();

	return 0;
}
