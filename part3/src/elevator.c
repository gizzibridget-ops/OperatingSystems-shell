#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cop4610t");
MODULE_DESCRIPTION("Office elevator kernel module");

extern long (*STUB_start_elevator)(void);
extern long (*STUB_issue_request)(int, int, int);
extern long (*STUB_stop_elevator)(void);

#define NUM_FLOORS     5
#define MAX_PASSENGERS 5
#define MAX_WEIGHT     70

static const int passenger_weight[] = { 10, 15, 20, 5 };
static const char passenger_char[]  = { 'P', 'L', 'B', 'V' };

typedef enum { OFFLINE, IDLE, LOADING, UP, DOWN } ElevatorState;

struct passenger {
    int type;
    int start_floor;
    int dest_floor;
    struct list_head list;
};

struct elevator_state {
    ElevatorState state;
    int floor;
    int weight;
    int num_passengers;
    int passengers_waiting;
    int passengers_serviced;
    int deactivating;
    struct list_head passengers;
    struct list_head floor_queues[NUM_FLOORS];
    struct mutex lock;
};

static struct elevator_state elev;
static struct task_struct *elevator_thread;
static struct proc_dir_entry *proc_entry;

/* Helpers */

static int has_work_above(void)
{
    struct passenger *p;
    int i;

    list_for_each_entry(p, &elev.passengers, list)
        if (p->dest_floor > elev.floor)
            return 1;

    for (i = elev.floor; i < NUM_FLOORS; i++)
        if (!list_empty(&elev.floor_queues[i]))
            return 1;

    return 0;
}

static int has_work_below(void)
{
    struct passenger *p;
    int i;

    list_for_each_entry(p, &elev.passengers, list)
        if (p->dest_floor < elev.floor)
            return 1;

    for (i = 0; i < elev.floor - 1; i++)
        if (!list_empty(&elev.floor_queues[i]))
            return 1;

    return 0;
}

static int has_stop_at_floor(void)
{
    struct passenger *p;

    /* Always stop if someone needs to get off */
    list_for_each_entry(p, &elev.passengers, list)
        if (p->dest_floor == elev.floor)
            return 1;

    /* Only stop to load if we have capacity */
    if (elev.deactivating)
        return 0;

    if (elev.num_passengers >= MAX_PASSENGERS)
        return 0;

    list_for_each_entry(p, &elev.floor_queues[elev.floor - 1], list)
        if (elev.weight + passenger_weight[p->type] <= MAX_WEIGHT)
            return 1;

    return 0;
}

static void do_unload(void)
{
    struct passenger *p, *tmp;

    list_for_each_entry_safe(p, tmp, &elev.passengers, list) {
        if (p->dest_floor == elev.floor) {
            list_del(&p->list);
            elev.weight -= passenger_weight[p->type];
            elev.num_passengers--;
            elev.passengers_serviced++;
            kfree(p);
        }
    }
}

static void do_load(void)
{
    struct passenger *p, *tmp;
    struct list_head *queue = &elev.floor_queues[elev.floor - 1];

    if (elev.deactivating)
        return;

    list_for_each_entry_safe(p, tmp, queue, list) {
        if (elev.num_passengers >= MAX_PASSENGERS)
            break;
        if (elev.weight + passenger_weight[p->type] > MAX_WEIGHT)
            break;
        list_del(&p->list);
        list_add_tail(&p->list, &elev.passengers);
        elev.weight += passenger_weight[p->type];
        elev.num_passengers++;
        elev.passengers_waiting--;
    }
}

/* Syscall implementations */

static long start_elevator(void)
{
    mutex_lock(&elev.lock);

    if (elev.state != OFFLINE) {
        mutex_unlock(&elev.lock);
        return 1;
    }

    elev.state          = IDLE;
    elev.floor          = 1;
    elev.weight         = 0;
    elev.num_passengers = 0;
    elev.deactivating   = 0;

    mutex_unlock(&elev.lock);
    return 0;
}

static long issue_request(int start_floor, int destination_floor, int type)
{
    struct passenger *p;

    if (start_floor < 1 || start_floor > NUM_FLOORS            ||
        destination_floor < 1 || destination_floor > NUM_FLOORS ||
        start_floor == destination_floor                         ||
        type < 0 || type > 3)
        return 1;

    p = kmalloc(sizeof(*p), GFP_KERNEL);
    if (!p)
        return -ENOMEM;

    p->type        = type;
    p->start_floor = start_floor;
    p->dest_floor  = destination_floor;
    INIT_LIST_HEAD(&p->list);

    mutex_lock(&elev.lock);
    list_add_tail(&p->list, &elev.floor_queues[start_floor - 1]);
    elev.passengers_waiting++;
    mutex_unlock(&elev.lock);

    return 0;
}

static long stop_elevator(void)
{
    mutex_lock(&elev.lock);

    if (elev.deactivating) {
        mutex_unlock(&elev.lock);
        return 1;
    }

    elev.deactivating = 1;

    mutex_unlock(&elev.lock);
    return 0;
}

/* Elevator kthread — scheduling algorithm */

static int elevator_run(void *data)
{
    int do_sleep = 0;
    int sleep_duration = 0;

    while (!kthread_should_stop()) {
        do_sleep = 0;

        mutex_lock(&elev.lock);

        switch (elev.state) {

        /*
         * OFFLINE: module loaded but elevator not started.
         */
        case OFFLINE:
            break;

        /*
         * IDLE: elevator is running but has no work right now.
         */
        case IDLE:
            if (elev.deactivating && elev.num_passengers == 0) {
                elev.state    = OFFLINE;
                elev.deactivating = 0;
                break;
            }

            if (!list_empty(&elev.floor_queues[elev.floor - 1])) {
                elev.state = LOADING;
                break;
            }

            if (has_work_above())
                elev.state = UP;
            else if (has_work_below())
                elev.state = DOWN;
            break;

        case LOADING:
            do_unload();
            do_load();
            do_sleep       = 1;
            sleep_duration = 1;

            if (elev.deactivating && elev.num_passengers == 0) {
                elev.state    = OFFLINE;
                elev.deactivating = 0;
            } else if (has_work_above()) {
                elev.state = UP;
            } else if (has_work_below()) {
                elev.state = DOWN;
            } else {
                elev.state = IDLE;
            }
            break;

        case UP:
            elev.floor++;
            do_sleep       = 1;
            sleep_duration = 2;

            if(has_stop_at_floor())
	    {
        	elev.state = LOADING;
 	    }
	    else if(has_work_above())
	    {
        	elev.state = UP;
    	    }
	    else if(has_work_below())
	    {
        	elev.state = DOWN;
    	    }
	    else
	    {
        	elev.state = IDLE;
    	    }
    	    break;

        case DOWN:
            elev.floor--;
            do_sleep       = 1;
            sleep_duration = 2;

            if(has_stop_at_floor())
	    {
        	elev.state = LOADING;
	    }
	    else if(has_work_below())
	    {
        	elev.state = DOWN;
	    }
	    else if(has_work_above())
	    {
        	elev.state = UP;
	    }
	    else
	    {
        	elev.state = IDLE;
    	    }
   	    break;
        }

        mutex_unlock(&elev.lock);

        if (do_sleep)
            ssleep(sleep_duration);
        else
            ssleep(1);
    }

    return 0;
}

/* /proc/elevator */

static int elevator_show(struct seq_file *m, void *v)
{
    static const char *state_names[] = {
        "OFFLINE", "IDLE", "LOADING", "UP", "DOWN"
    };
    struct passenger *p;
    int i;

    mutex_lock(&elev.lock);

    seq_printf(m, "Elevator state: %s\n", state_names[elev.state]);
    seq_printf(m, "Current floor: %d\n", elev.floor);
    seq_printf(m, "Current load: %d lbs\n", elev.weight);

    seq_printf(m, "Elevator status:");
    list_for_each_entry(p, &elev.passengers, list)
        seq_printf(m, " %c%d", passenger_char[p->type], p->dest_floor);
    seq_putc(m, '\n');

    seq_putc(m, '\n');
    for (i = NUM_FLOORS; i >= 1; i--) {
        int count = 0;

        /*
         * Count waiting passengers on this floor first so we can
         * print the count before the passenger list
         */
        list_for_each_entry(p, &elev.floor_queues[i - 1], list)
            count++;

        seq_printf(m, "[%c] Floor %d: %d",
                   elev.floor == i ? '*' : ' ', i, count);

        list_for_each_entry(p, &elev.floor_queues[i - 1], list)
            seq_printf(m, " %c%d", passenger_char[p->type], p->dest_floor);

        seq_putc(m, '\n');
    }

    seq_putc(m, '\n');
    seq_printf(m, "Number of passengers: %d\n", elev.num_passengers);
    seq_printf(m, "Number of passengers waiting: %d\n", elev.passengers_waiting);
    seq_printf(m, "Number of passengers serviced: %d\n", elev.passengers_serviced);

    mutex_unlock(&elev.lock);
    return 0;
}

static int elevator_open(struct inode *inode, struct file *file)
{
    return single_open(file, elevator_show, NULL);
}

static const struct proc_ops elevator_fops = {
    .proc_open    = elevator_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* Module init / exit */

static int __init elevator_init(void)
{
    int i;

    mutex_init(&elev.lock);
    elev.state               = OFFLINE;
    elev.floor               = 1;
    elev.weight              = 0;
    elev.num_passengers      = 0;
    elev.passengers_waiting  = 0;
    elev.passengers_serviced = 0;
    elev.deactivating        = 0;

    INIT_LIST_HEAD(&elev.passengers);
    for (i = 0; i < NUM_FLOORS; i++)
        INIT_LIST_HEAD(&elev.floor_queues[i]);

    proc_entry = proc_create("elevator", 0444, NULL, &elevator_fops);
    if (!proc_entry)
        return -ENOMEM;

    elevator_thread = kthread_run(elevator_run, NULL, "elevator_thread");
    if (IS_ERR(elevator_thread)) {
        proc_remove(proc_entry);
        return PTR_ERR(elevator_thread);
    }

    STUB_start_elevator = start_elevator;
    STUB_issue_request  = issue_request;
    STUB_stop_elevator  = stop_elevator;

    return 0;
}

static void __exit elevator_exit(void)
{
    struct passenger *p, *tmp;
    int i;

    /* Unregister function pointers before stopping the thread */
    STUB_start_elevator = NULL;
    STUB_issue_request = NULL;
    STUB_stop_elevator = NULL;

    kthread_stop(elevator_thread);
    proc_remove(proc_entry);

    /* Free any passengers still on the elevator */
    list_for_each_entry_safe(p, tmp, &elev.passengers, list) {
        list_del(&p->list);
        kfree(p);
    }

    /* Free any passengers still waiting on floors */
    for (i = 0; i < NUM_FLOORS; i++) {
        list_for_each_entry_safe(p, tmp, &elev.floor_queues[i], list) {
            list_del(&p->list);
            kfree(p);
        }
    }

    mutex_destroy(&elev.lock);
}

module_init(elevator_init);
module_exit(elevator_exit);
