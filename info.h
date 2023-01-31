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

#define AUDIT if (1)

//#define TEST_BLOCKING_OPS

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


typedef struct _object_state
{
        struct mutex operation_synchronizer;
        char *buffer; //
        int valid_bytes;
        wait_queue_head_t wq;
} object_state;


typedef struct _session
{

        int priority;         // priority level (high or low) for the operations
        int blocking;         // blocking vs non-blocking read and write operations
        unsigned long timeout; // setup of a timeout regulating the awake of blocking operations

} session;


object_state objects[MINORS][DATA_FLOWS];

#endif
