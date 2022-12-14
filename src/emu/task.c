/* Copyright (c) 2022 Barcelona Supercomputing Center (BSC)
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "uthash.h"
#include "utlist.h"

#include "chan.h"
#include "emu.h"
#include "task.h"
#include "ovni.h"
#include "prv.h"

struct task *
task_find(struct task *tasks, uint32_t task_id)
{
	struct task *task = NULL;
	HASH_FIND_INT(tasks, &task_id, task);

	return task;
}

struct task_type *
task_type_find(struct task_type *types, uint32_t type_id)
{
	struct task_type *type = NULL;
	HASH_FIND_INT(types, &type_id, type);

	return type;
}

void
task_create(struct ovni_emu *emu, struct task_info *info,
		uint32_t type_id, uint32_t task_id)
{
	/* Ensure the task id is new */
	if (task_find(info->tasks, task_id) != NULL)
		edie(emu, "cannot create task: task_id %u already exists\n",
				task_id);

	/* Ensure the type exists */
	struct task_type *type = task_type_find(info->types, type_id);
	if (type == NULL)
		edie(emu, "cannot create task: unknown type id %u\n", type_id);

	struct task *task = calloc(1, sizeof(struct task));

	if (task == NULL)
		die("calloc failed\n");

	task->id = task_id;
	task->type = type;
	task->state = TASK_ST_CREATED;
	task->thread = NULL;

	/* Add the new task to the hash table */
	HASH_ADD_INT(info->tasks, id, task);

	dbg("new task created id=%d\n", task->id);
}

void
task_execute(struct ovni_emu *emu,
		struct task_stack *stack, struct task *task)
{
	if (task == NULL)
		edie(emu, "cannot execute: task is NULL\n");

	if (task->state != TASK_ST_CREATED)
		edie(emu, "cannot execute task %u: state is not created\n", task->id);

	if (task->thread != NULL)
		edie(emu, "task already has a thread assigned\n");

	if (stack->thread->state != TH_ST_RUNNING)
		edie(emu, "thread state is not running\n");

	if (stack->top == task)
		edie(emu, "thread already has assigned task %u\n", task->id);

	if (stack->top && stack->top->state != TASK_ST_RUNNING)
		edie(emu, "cannot execute a nested task from a non-running task\n");

	task->state = TASK_ST_RUNNING;
	task->thread = stack->thread;

	DL_PREPEND(stack->tasks, task);

	dbg("task id=%u runs now\n", task->id);
}

void
task_pause(struct ovni_emu *emu,
		struct task_stack *stack, struct task *task)
{
	if (task == NULL)
		edie(emu, "cannot pause: task is NULL\n");

	if (task->state != TASK_ST_RUNNING)
		edie(emu, "cannot pause: task state is not running\n");

	if (task->thread == NULL)
		edie(emu, "cannot pause: task has no thread assigned\n");

	if (stack->thread->state != TH_ST_RUNNING)
		edie(emu, "cannot pause: thread state is not running\n");

	if (stack->top != task)
		edie(emu, "thread has assigned a different task\n");

	if (stack->thread != task->thread)
		edie(emu, "task is assigned to a different thread\n");

	task->state = TASK_ST_PAUSED;

	dbg("task id=%d pauses\n", task->id);
}

void
task_resume(struct ovni_emu *emu,
		struct task_stack *stack, struct task *task)
{
	if (task == NULL)
		edie(emu, "cannot resume: task is NULL\n");

	if (task->state != TASK_ST_PAUSED)
		edie(emu, "task state is not paused\n");

	if (task->thread == NULL)
		edie(emu, "cannot resume: task has no thread assigned\n");

	if (stack->thread->state != TH_ST_RUNNING)
		edie(emu, "thread is not running\n");

	if (stack->top != task)
		edie(emu, "thread has assigned a different task\n");

	if (stack->thread != task->thread)
		edie(emu, "task is assigned to a different thread\n");

	task->state = TASK_ST_RUNNING;

	dbg("task id=%d resumes\n", task->id);
}

void
task_end(struct ovni_emu *emu,
		struct task_stack *stack, struct task *task)
{
	if (task == NULL)
		edie(emu, "cannot end: task is NULL\n");

	if (task->state != TASK_ST_RUNNING)
		edie(emu, "task state is not running\n");

	if (task->thread == NULL)
		edie(emu, "cannot end: task has no thread assigned\n");

	if (stack->thread->state != TH_ST_RUNNING)
		edie(emu, "cannot end task: thread is not running\n");

	if (stack->top != task)
		edie(emu, "thread has assigned a different task\n");

	if (stack->thread != task->thread)
		edie(emu, "task is assigned to a different thread\n");

	task->state = TASK_ST_DEAD;

	/* Don't unset the thread from the task, as it will be used
	 * later to ensure we switch to tasks of the same thread. */

	DL_DELETE(stack->tasks, task);

	dbg("task id=%d ends\n", task->id);
}

static uint32_t
get_task_type_gid(const char *label)
{
	uint32_t gid;

	HASH_VALUE(label, strlen(label), gid);

	/* Use non-negative values */
	gid &= 0x7FFFFFFF;

	/* Avoid bad colors for "Unlabeled0" and "main" */
	gid += 666;

	if (gid == 0)
		gid++;

	return gid;
}

void
task_type_create(struct task_info *info, uint32_t type_id, const char *label)
{
	struct task_type *type;
	/* Ensure the type id is new */
	HASH_FIND_INT(info->types, &type_id, type);

	if (type != NULL)
		die("a task type with id %u already exists\n", type_id);

	type = calloc(1, sizeof(*type));

	if (type == NULL)
		die("calloc failed");

	type->id = type_id;

	if (type->id == 0)
		die("invalid task type id %d\n", type->id);

	type->gid = get_task_type_gid(label);
	int n = snprintf(type->label, MAX_PCF_LABEL, "%s", label);
	if (n >= MAX_PCF_LABEL)
		die("task type label too long: %s\n", label);

	/* Add the new task type to the hash table */
	HASH_ADD_INT(info->types, id, type);

	dbg("new task type created id=%d label=%s\n", type->id,
			type->label);
}

void
task_create_pcf_types(struct pcf_type *pcftype, struct task_type *types)
{
	/* Emit types for all task types */
	for (struct task_type *tt = types; tt != NULL; tt = tt->hh.next) {
		struct pcf_value *pcfvalue = pcf_find_value(pcftype, tt->gid);
		if (pcfvalue != NULL) {
			/* Ensure the label is the same, so we know that
			 * no collision occurred */
			if (strcmp(pcfvalue->label, tt->label) != 0)
				die("collision occurred in task type labels\n");
			else
				continue;
		}

		pcf_add_value(pcftype, tt->gid, tt->label);
	}
}

struct task *
task_get_running(struct task_stack *stack)
{
	struct task *task = stack->top;
	if (task && task->state == TASK_ST_RUNNING)
		return task;

	return NULL;
}
