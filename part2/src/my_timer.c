#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cop4610t");
MODULE_DESCRIPTION("Kernel module that displays current and elapsed time");

#define ENTRY_NAME "timer"
#define PERMS 0644
#define PARENT NULL

static struct proc_dir_entry *timer_entry;

static struct timespec64 last_time; //store time last read

static int first_read = 1; //track if read has happened 

static ssize_t timer_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
    char buf[256];
    int len = 0;
    struct timespec64 current_time;
    struct timespec64 elapsed;

    ktime_get_real_ts64(&current_time); //get curr time

    len += sprintf(buf + len, "current time: %lld.%09ld\n", //print curr time, seconds & nano
                   (long long)current_time.tv_sec,
                   current_time.tv_nsec);

    //only print elapsed time if not first read 
    if (!first_read) {
        elapsed.tv_sec  = current_time.tv_sec  - last_time.tv_sec;
        elapsed.tv_nsec = current_time.tv_nsec - last_time.tv_nsec;

        //handle nanosecond underflow 
        if (elapsed.tv_nsec < 0) {
            elapsed.tv_sec  -= 1;
            elapsed.tv_nsec += 1000000000L;
        }

        len += sprintf(buf + len, "elapsed time: %lld.%09ld\n",
                       (long long)elapsed.tv_sec,
                       elapsed.tv_nsec);
    }

    //save curr time as the new last time for next read
    last_time  = current_time;
    first_read = 0;

    return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct proc_ops timer_fops = {
    .proc_read = timer_read,
};

static int __init timer_init(void)
{
    timer_entry = proc_create(ENTRY_NAME, PERMS, PARENT, &timer_fops);
    if (!timer_entry) {
        return -ENOMEM;
    }
    return 0;
}

static void __exit timer_exit(void)
{
    proc_remove(timer_entry);
}

module_init(timer_init);
module_exit(timer_exit);
