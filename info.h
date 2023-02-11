#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/tty.h>     /* For the tty declarations */
#include <linux/version.h> /* For LINUX_VERSION_CODE */
#include <linux/moduleparam.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

MODULE_AUTHOR("Gianmarco Bencivenni");
MODULE_DESCRIPTION("Multi-flow device file");
MODULE_LICENSE("GPL");

#define MODNAME "MULTI-FLOW-DEVICE-FILE"
#define DEVICE_NAME "my-device" /* Device file name in /dev/ - not mandatory  */

#define MINORS 128
#define DATA_FLOWS 2
#define LOW_PRIORITY 0
#define HIGH_PRIORITY 1
#define BLOCKING 0
#define NON_BLOCKING 1
#define OBJECT_MAX_SIZE  (4096) //just one page
#define MIN(a,b) (((a)<(b))?(a):(b))
#define AUDIT if (1)

#ifndef _INFOH_
#define _INFOH_

static int disabled_device[MINORS];
module_param_array(disabled_device, int, NULL, 0660);
MODULE_PARM_DESC(disabled_device, "Parameter to enable or disable " \
"the device file, in terms of a specific minor number. If it is disabled, " \
"any attempt to open a session should fail (but already open sessions will still be managed).");

static int hp_bytes[MINORS];
module_param_array(hp_bytes, int, NULL, 0660);
MODULE_PARM_DESC(hp_bytes, "Current number of bytes in the High Priority flow.");

static int lp_bytes[MINORS];
module_param_array(lp_bytes, int, NULL, 0660);
MODULE_PARM_DESC(lp_bytes, "Current number of bytes in the Low Priority flow.");

static int hp_threads[MINORS];
module_param_array(hp_threads, int, NULL, 0660);
MODULE_PARM_DESC(hp_threads, "Number of threads currently waiting for data along the High Priority flow.");

static int lp_threads[MINORS];
module_param_array(lp_threads, int, NULL, 0660);
MODULE_PARM_DESC(lp_threads, "Number of threads currently waiting for data along the Low Priority flow.");


typedef struct _data_segment
{
        char *buffer;
        size_t actual_size;
        off_t  off;
        struct _data_segment *next;
        struct _data_segment *previous;
        
} data_segment;


typedef struct _object_state
{
        struct mutex operation_synchronizer;
        data_segment *head;                     // head of the linked list of data segments.
        data_segment *tail;                     // tail of the linked list of data segments.
        int valid_bytes;
        int pending_bytes;
        wait_queue_head_t wq;

} object_state;


typedef struct _session
{
        int priority;                           // priority level (high or low) for the operations
        int blocking;                           // blocking vs non-blocking read and write operations
        unsigned long timeout;                  // setup of a timeout regulating the awake of blocking operations

} session;


object_state objects[MINORS][DATA_FLOWS];



int writable_bytes( object_state *the_object, int priority ) {
   if (priority == HIGH_PRIORITY)
      return OBJECT_MAX_SIZE - (the_object -> valid_bytes);
   else
      return OBJECT_MAX_SIZE - (the_object -> valid_bytes) - (the_object -> pending_bytes);
}


int check_if_writable_and_try_lock(object_state *the_object, int priority) {
   if(mutex_trylock(&(the_object -> operation_synchronizer))) {
      if(writable_bytes(the_object, priority) == 0) {
            mutex_unlock(&(the_object -> operation_synchronizer));
            return 0;
      }
      return 1;
   }
   return 0;
}


int check_if_readable_and_try_lock(object_state *the_object) {
        if (mutex_trylock(&(the_object->operation_synchronizer))) {
                if(the_object->valid_bytes == 0) {
                        mutex_unlock(&(the_object->operation_synchronizer));
                        return 0;
                }
                return 1;
        }
        return 0;
}

void inc_pending_threads( int minor, int priority ) {
   if (priority == HIGH_PRIORITY)
      __sync_add_and_fetch(&hp_threads[minor], 1);
   else 
      __sync_add_and_fetch(&lp_threads[minor], 1);
}


void dec_pending_threads( int minor, int priority ) {
   if (priority == HIGH_PRIORITY)
      __sync_add_and_fetch(&hp_threads[minor], -1);
   else 
      __sync_add_and_fetch(&lp_threads[minor], -1);
}


ssize_t free_data_segment( data_segment *segment, ssize_t error ) {
   if (likely(segment -> buffer != NULL))
            kfree(segment -> buffer);
   kfree(segment);
   return -error;
}


#endif
