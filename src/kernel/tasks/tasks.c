#include "tasks.h"

static void task_setup_ldt (task_t * task);

void task_list_reset () {
	int * countBuf = kTaskCount;
	int * currBuf = kTaskCurrent;
	pid_t * pid = kTaskPID;
	*countBuf = 0;
	*currBuf = -1;
	*pid = 0;
}

int task_list_count () {
	int * countBuf = kTaskCount;
	int c = *countBuf;
	return c;
}

pid_t task_list_current () {
	int * currBuf = kTaskCurrent;
	if (*currBuf == -1) return 0;
	unsigned int offset = sizeof(task_t) * (*currBuf);
	task_t * task = (task_t *)((unsigned int)(kTaskListBase) + offset);
	return task->pid;
}

osStatus task_get (pid_t pid, task_t * destination) {
	int * countBuf = kTaskCount;
	int i;
	for (i = 0; i < *countBuf; i++) {
		unsigned int offset = sizeof(task_t) * i;
		task_t * task = (task_t *)((unsigned int)(kTaskListBase) + offset);
		if (task->pid == pid) {
			kmemcpy((char *)destination, (char *)task, sizeof(task_t));
			return osOK;
		}
	}
	return osGeneralError;
}

pid_t task_next_pid () {
	int * pidBuf = kTaskPID;
	*pidBuf += 1;
	return *pidBuf;
}

osStatus task_kill (pid_t pid) {
	// remove this from the task list.
	int * countBuf = kTaskCount;
	int i, numGone = 0;
	task_t * nextTask = kTaskListBase;
	task_t * dstTask = kTaskListBase;
	for (i = 0; i < *countBuf; i++) {
		if (nextTask->pid == pid) {
			numGone += 1;
		} else {
			kmemcpy((char *)dstTask, (char *)nextTask, sizeof(task_t));
			dstTask += sizeof(task_t);
		}
		nextTask += sizeof(task_t);
	}
	*countBuf -= numGone;
	if (numGone == 1) return osOK;
	return osGeneralError;
}

osStatus task_start (char * codeBase, unsigned short length, pid_t * pid) {
	// find the next available program space
	void * baseAddr = kTaskSpaceStart;
	int * countBuf = kTaskCount;
	int i;
	bool isGood = true;
	while (true) {
		// check if it's good
		for (i = 0; i < *countBuf; i++) {
			unsigned int offset = sizeof(task_t) * i;
			task_t * task = (task_t *)((unsigned int)kTaskListBase + offset);
			if (task->basePtr == baseAddr) {
				isGood = false;
				break;
			}
		}
		if (isGood) {
			break;
		} else baseAddr += kTaskSpacePerKernTask + kTaskSpacePerTask;
	}
	// create our task
	task_t * ourTask = *countBuf * sizeof(task_t) + kTaskListBase;
	for (i = 0; i < sizeof(task_t); i++) {
		((char *)ourTask)[i] = 0;
	}
	ourTask->basePtr = baseAddr;
	task_setup_ldt(ourTask);
	return osOK;
}

static void task_setup_ldt (task_t * task) {
	// LDT code selector
	// 	size - 1
	task->ldt[0] = 0xff;
	task->ldt[1] = 0xff;
	//  base (byte 0, 1, 2)
	task->ldt[2] = ((unsigned int)task->basePtr & 0xff);
	task->ldt[3] = ((unsigned int)task->basePtr >> 8) & 0xff;
	task->ldt[4] = ((unsigned int)task->basePtr >> 16) & 0xff;
	//  privilege level 3 (code segment flags)
	task->ldt[5] = 0xfa;
	//  AVL, 0, D, G
	task->ldt[6] = 0xcf;
	task->ldt[7] = ((unsigned int)task->basePtr >> 24) & 0xff;
	// LDT data selector (same as code)
	task->ldt[8] = task->ldt[0];
	task->ldt[9] = task->ldt[1];
	task->ldt[10] = task->ldt[2];
	task->ldt[11] = task->ldt[3];
	task->ldt[12] = task->ldt[4];
	//  privilege level 3 (data segment flags)
	task->ldt[13] = 0xf2;
	task->ldt[14] = task->ldt[6];
	task->ldt[15] = task->ldt[7];

	// set the task's segments to point to the LDT
	task->cs = 0x07; // rpl = 3, LDT = true, segment = 0
	task->ds = 0x0f; // rpl = 3, LDT = true, segment = 1
	task->gs = task->ds;
	task->es = task->ds;
	task->fs = task->ds;
	task->ss = task->ds;
	task->esp = 0xffff;
	task->ebp = 0xffff;
}

