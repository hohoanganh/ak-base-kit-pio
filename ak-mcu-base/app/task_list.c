#include "task_list.h"
#include "timer.h"

task_t app_task_table[] = {
	/*************************************************************************/
	/* SYSTEM TASK */
	/*************************************************************************/
	{TASK_TIMER_TICK_ID,	TASK_PRI_LEVEL_7,		task_timer_tick	},

	/*************************************************************************/
	/* APP TASK */
	/*************************************************************************/
	{TASK_SYSTEM_ID,		TASK_PRI_LEVEL_2,		task_system		},
	{TASK_CONSOLE_ID,		TASK_PRI_LEVEL_3,		task_console	},
	{TASK_FW_ID,			TASK_PRI_LEVEL_4,		task_fw			},

	/*************************************************************************/
	/* END OF TABLE */
	/*************************************************************************/
	{AK_TASK_EOT_ID,		TASK_PRI_LEVEL_0,		(pf_task)0		}
};

task_polling_t app_task_polling_table[] = {
	{TASK_POLL_CONSOLE_ID,	AK_ENABLE,		task_poll_console	},
	{AK_TASK_POLLING_EOT_ID,AK_DISABLE,		(pf_task_polling)0	},
};
