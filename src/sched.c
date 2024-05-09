
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return 0; // not empty -> 0

	return  1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
	int i ;

	for (i = 0; i < MAX_PRIO; i++) {
		mlq_ready_queue[i].size = 0;
		mlq_ready_queue[i].slot = MAX_PRIO - i;
	}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
void resetSlot() { //OUR FUNCTION
	for (int i = 0; i < MAX_PRIO; ++i) {
		mlq_ready_queue[i].slot = MAX_PRIO - i;
	}
}
struct pcb_t * get_mlq_proc_recursive(int milestone) {
    // Lock the queue to protect it
    pthread_mutex_lock(&queue_lock);

    // Check if the queue is empty
    if (queue_empty() == 1) {
        pthread_mutex_unlock(&queue_lock);
        return NULL;
    }

    // Check if the milestone exceeds the maximum priority
    if (milestone >= MAX_PRIO) {
        resetSlot();
        milestone = 0;
    }

    // Check if the current priority level has processes
    int flag = empty(&mlq_ready_queue[milestone]);
    if (flag) {
        // If the current priority level is empty, continue to the next level
        pthread_mutex_unlock(&queue_lock);
        return get_mlq_proc_recursive(milestone + 1);
    }

    // Dequeue a process from the current priority level
    struct pcb_t * proc = dequeue(&mlq_ready_queue[milestone]);
    mlq_ready_queue[milestone].slot--;

    // Unlock the queue and return the dequeued process
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

// Wrapper function to call get_mlq_proc_recursive with initial milestone
struct pcb_t * get_mlq_proc(void) {
    return get_mlq_proc_recursive(0);
}

void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	pthread_mutex_lock(&queue_lock);
	if (empty(&ready_queue))
	{
		while (!empty(run_queue))
		{
			enqueue(&ready_queue, dequeue(&run_queue));
		}
	}
	proc = dequeue(&ready_queue);
	pthread_mutex_unlock(&queue_lock);

	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif

