/*
 * Copyright(C) 2011-2016 Pedro H. Penna   <pedrohenriquepenna@gmail.com>
 *              2015-2016 Davidson Francis <davidsondfgl@hotmail.com>
 *
 * This file is part of Nanvix.
 *
 * Nanvix is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nanvix is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nanvix. If not, see <http://www.gnu.org/licenses/>.
 */

#include <nanvix/clock.h>
#include <nanvix/const.h>
#include <nanvix/hal.h>
#include <nanvix/pm.h>
#include <signal.h>

#include <nanvix/klib.h>

// PRIVATE struct process* ready_processes[PROC_MAX]; // Array to store pointers to ready processes
// PRIVATE int ready_count = 0; // Count of ready processes


// PRIVATE int exists(struct process* proc) {
//     for (int i = 0; i < ready_count; i++) {
//         if (ready_processes[i] == proc) {
//             return 1; // Process found in the ready list
//         }
//     }
//     return 0; // Process not found
// }

// // Function to add a process to the ready list
// PRIVATE void add_to_ready_list(struct process* proc) {
//     if (!exists(proc)) {
//         ready_processes[ready_count] = proc;
// 		ready_count++;
//     }
// }

// PRIVATE void remove_from_ready_list(struct process* proc) {
//     for (int i = 0; i < ready_count; i++) {
//         if (ready_processes[i] == proc) {
//             // Shift remaining processes down one position
//             for (int j = i; j < ready_count - 1; j++) {
//                 ready_processes[j] = ready_processes[j + 1];
//             }
//             ready_count--;
//             return;
//         }
//     }
// }

PUBLIC int total_nb_tickets = 0;
PUBLIC int nReady = 0;
/**
 * @brief Schedules a process to execution.
 *
 * @param proc Process to be scheduled.
 */
PUBLIC void sched(struct process *proc)
{
	//add_to_ready_list(proc);
	proc->state = PROC_READY;
	proc->counter = 0;
}

/**
 * @brief Stops the current running process.
 */
PUBLIC void stop(void)
{
	//remove_from_ready_list(curr_proc);
	curr_proc->state = PROC_STOPPED;
	sndsig(curr_proc->father, SIGCHLD);
	yield();
}

/**
 * @brief Resumes a process.
 *
 * @param proc Process to be resumed.
 *
 * @note The process must stopped to be resumed.
 */
PUBLIC void resume(struct process *proc)
{
	/* Resume only if process has stopped. */
	if (proc->state == PROC_STOPPED)
		sched(proc);
}

//enum with FIFO, PRIO, RRORBIN
// Define the enum
typedef enum {FIFO, PRIO, RRORBIN, RANDOM, LOTTERY} ProcessMethod;

PRIVATE void fifo(){
	struct process *next = IDLE;
	struct process *p;
	for (p=FIRST_PROC; p <= LAST_PROC; p++)
	{
		/* Skip non-ready process. */
		if (p->state != PROC_READY)
			continue;

		/*
		 * Process with higher
		 * waiting time found.
		 */
		if (p->counter > next->counter)
		{
			next->counter++;
			next = p;
		}

		/*
		 * Increment waiting
		 * time of process.
		 */
		else
			p->counter++;
	}

	/* Switch to next process. */
	next->priority = PRIO_USER;
	next->state = PROC_RUNNING;
	next->counter = PROC_QUANTUM;
	if (curr_proc != next)
		switch_to(next);
}

PRIVATE void prio(){
	struct process *next = IDLE;
	struct process *p;
	for (p=FIRST_PROC; p <= LAST_PROC; p++)
	{
		/* Skip non-ready process. */
		if (p->state != PROC_READY || p == IDLE)
			continue;
		
		/*
		 * Process with higher
		 * priority found.
		 */
		if (p->nice < next->nice || next == IDLE)
		{
			next = p;
		}

		else if (p->nice == next->nice)
		{
			if (p->utime + p->ktime < next->utime + next->ktime)
			{
				next = p;
			}
		}
	}

	/* Switch to next process. */
	next->priority = PRIO_USER;
	next->state = PROC_RUNNING;
	next->counter = PROC_QUANTUM;
	if (curr_proc != next)
		switch_to(next);
}

PRIVATE void rrorbin(){
	struct process *next = IDLE;
    struct process *p = curr_proc;
    while (TRUE) {
        p++;  // Move to the next process

        // Wrap around if we reach the end
        if (p > LAST_PROC) {
            p = FIRST_PROC;
        }

        // Check if the process is ready
        if (p->state == PROC_READY) {
            next = p;
            break;  // Found the next process to execute
        }

		if(p == curr_proc){
			break;
		}
    }

    // Switch to the next process
    next->priority = PRIO_USER;
    next->state = PROC_RUNNING;
    next->counter = PROC_QUANTUM;
    if (curr_proc != next)
        switch_to(next);
}


//tried using an array to optimize it but it doesn't work
// PRIVATE void random() {
// 	struct process* p;
// 	int i = ready_count;
// 	srand(1); // Seed the random number generator with the current time
//     if (i> 0) {
//         int idx = krand() % i; // Select a random index within the ready list
//         p = ready_processes[idx];
//     }
// 	else{
// 		p = IDLE;
// 	}

// 	p->priority = PRIO_USER;
// 	p->state = PROC_RUNNING;
// 	p->counter = PROC_QUANTUM;
// 	if (curr_proc != p)
// 		switch_to(p);
// }


PRIVATE void random() {
    struct process *p;
    
    if (nReady > 0) {
		ksrand(1); // Seed the random number generator with the current time
        int target = krand() % nReady; // Select a target index at random
        int i = 0; // Reset counter to reuse for indexing
        
        // Second pass: Find the target process
        for (p = FIRST_PROC; p <= LAST_PROC; p++) {
            if (p->state == PROC_READY) {
                if (i == target) {
                    // This is the selected process
                    p->priority = PRIO_USER;
                    p->state = PROC_RUNNING;
                    p->counter = PROC_QUANTUM;
                    if (curr_proc != p)
                        switch_to(p);
                    return; // Exit after switching to the selected process
                }
                i++;
            }
        }
    }
	p = IDLE;
	p->priority = PRIO_USER;
    p->state = PROC_RUNNING;
    p->counter = PROC_QUANTUM;
    if (curr_proc != p)
        switch_to(p);
}

PRIVATE void lottery(){
	struct process *p;
    struct process *next = IDLE;
    int plage_debut = 0;
    int ticket_gagnant = ticks % total_nb_tickets;
    
    for (p = FIRST_PROC; p <= LAST_PROC; p++)
    {
        if (p->state != PROC_READY)
            continue;

        int plage_fin = plage_debut + 100 + 40 - (p->priority) - (p->nice)+p->counter;
        
        if(plage_fin > ticket_gagnant){
            if(next != IDLE) next->counter++;
            next = p;
        } else {
            if(p != IDLE){
                p->counter++;
            }
            
        }
        plage_debut = plage_fin;
    }

    /* Switch to next process. */
    next->priority = PRIO_USER;
    next->state = PROC_RUNNING;
    next->counter = PROC_QUANTUM;
    if (curr_proc != next)
        switch_to(next);
}

PUBLIC ProcessMethod method = RANDOM;

/**
 * @brief Yields the processor.
 */
PUBLIC void yield(void)
{
	struct process *p;    /* Working process.     */
	//struct process *next; /* Next process to run. */

	/* Re-schedule process for execution. */
	if (curr_proc->state == PROC_RUNNING)
		sched(curr_proc);

	/* Remember this process. */
	last_proc = curr_proc;
	total_nb_tickets = 0;
	nReady = 0;
	/* Check alarm. */
	for (p = FIRST_PROC; p <= LAST_PROC; p++)
	{
		/* Skip invalid processes. */
		if (!IS_VALID(p))
			continue;

		/* Alarm has expired. */
		if ((p->alarm) && (p->alarm < ticks))
			p->alarm = 0, sndsig(p, SIGALRM);

		total_nb_tickets += 100 + 40 - (p->priority) - (p->nice)+p->counter;

		if (p->state == PROC_READY)
			nReady++;
	}

	/* Choose a process to run next. */
	switch(method){
		case FIFO:
			fifo();
			break;
		case PRIO:
			prio();
			break;
		case RRORBIN:
			rrorbin();
			break;
		case RANDOM:
			random();
			break;
		case LOTTERY:
			lottery();
			break;
		default:
			break;
	}
}

