/* Task table for kernel tests. */
#ifndef __TASK_LIST_H__
#define __TASK_LIST_H__

#include "ak.h"
#include "task.h"

enum {
	TASK_TIMER_TICK_ID,
	TASK_LOW_ID,
	TASK_MID_ID,
	TASK_HIGH_ID,
	AK_TASK_EOT_ID,
};

enum {
	TASK_POLL_A_ID,
	AK_TASK_POLLING_EOT_ID,
};

#endif
