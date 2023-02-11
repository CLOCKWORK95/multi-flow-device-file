#include "info.h"
#include "read.h"
#include "write.h"

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static long dev_ioctl(struct file *, unsigned int, unsigned long);

static int Major; /* Major number assigned to char device driver */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session) MAJOR(session->f_inode->i_rdev)
#define get_minor(session) MINOR(session->f_inode->i_rdev)
#else
#define get_major(session) MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session) MINOR(session->f_dentry->d_inode->i_rdev)
#endif


static int dev_open(struct inode *inode, struct file *file) {

   session *session;
   int minor = get_minor(file);
   
   if (minor >= MINORS) return -ENODEV;

   if(disabled_device[minor]){

      AUDIT printk("%s: dev with [minor] number [%d] disabled\n", MODNAME, minor);
      return -ENOENT;
   }

   session = kzalloc(sizeof(session), GFP_ATOMIC);
   AUDIT printk("%s: allocated new session\n", MODNAME);
   if (session == NULL)
   {
      printk("%s: unable to allocate new session\n", MODNAME);
      return -ENOMEM;
   }

   session->priority = HIGH_PRIORITY;
   session->blocking = NON_BLOCKING;
   session->timeout = 0;
   file->private_data = session;

   AUDIT printk("%s: device file successfully opened for object with minor %d\n", MODNAME, minor);
   
   return 0;
}


static int dev_release(struct inode *inode, struct file *file) {

   session *session = file->private_data;
   kfree(session);

   AUDIT printk("%s: device file closed\n", MODNAME);
   
   return 0;
}


static ssize_t dev_write(struct file *filp, const char __user *buff, size_t len, loff_t *off) {

   int ret, res, priority, blocking, minor, major;
   gfp_t flags;
   data_segment *new_segment;
   object_state *current_stream_state;
   session *session;

   minor = get_minor(filp);
   major = get_major(filp);
   
   session = filp -> private_data;
   priority = session -> priority;
   blocking = session -> blocking;

   current_stream_state = &objects[minor][priority];

   if (unlikely(len == 0))
            return 0;

   flags = (blocking == BLOCKING) ? GFP_KERNEL : GFP_ATOMIC;

   new_segment = (data_segment *) kzalloc(sizeof(data_segment), flags);
   if (unlikely(new_segment == NULL))
            return -ENOMEM;

   new_segment -> buffer = (char *) kzalloc(len, flags);
   if (unlikely(new_segment -> buffer == NULL))
            return free_data_segment(new_segment, ENOMEM);

   res = copy_from_user(new_segment -> buffer, buff, len);
   
   if (unlikely(res == len))
            return free_data_segment(new_segment, ENOMEM);

   if(blocking == BLOCKING) {
      
      AUDIT printk("%s current thread is going to wait for space available for writing on device %s [MAJOR: %d, minor: %d]",
            MODNAME, DEVICE_NAME, major, minor);

      inc_pending_threads(minor,priority);
      ret = wait_event_interruptible_timeout(
                     current_stream_state -> wq,
                     check_if_writable_and_try_lock(current_stream_state, priority),
                     msecs_to_jiffies(session -> timeout)
               );
      dec_pending_threads(minor,priority);

      AUDIT printk("%s current thread has waken up from wait queue related to device %s [MAJOR: %d, minor: %d]",
            MODNAME, DEVICE_NAME, major, minor);

      if(ret == 0) {
         AUDIT printk("%s timer has expired for current thread and cannot write on device %s [MAJOR: %d, minor: %d]",
            MODNAME, DEVICE_NAME, major, minor);

         return free_data_segment(new_segment, ETIME);

      } else if(ret == -ERESTARTSYS) {
         AUDIT printk("%s current thread received a signal while waiting for space on device %s [MAJOR: %d, minor: %d]",
            MODNAME, DEVICE_NAME, major, minor);

         return free_data_segment(new_segment, EINTR);
      }
   } else {
            if (!mutex_trylock(&(current_stream_state -> operation_synchronizer)))
                     return free_data_segment(new_segment, EBUSY)                   ;

            if (unlikely(writable_bytes(current_stream_state, priority) == 0)) {
                     mutex_unlock(&(current_stream_state -> operation_synchronizer));
                     return free_data_segment(new_segment, EAGAIN);
            }
   }

   new_segment-> actual_size = MIN(len - res, writable_bytes(current_stream_state, priority));

   if (priority == HIGH_PRIORITY) {
            ret = write( new_segment, current_stream_state );
   } else {
            if ((ret = put_work(current_stream_state, new_segment, major, minor, flags)) < 0) {
                     mutex_unlock(&(current_stream_state->operation_synchronizer));
                     // It gives the possibility to other threads to try to write
                     wake_up_interruptible(&(current_stream_state -> wq));
                     return free_data_segment(new_segment, -ret);
            }
   }

   mutex_unlock(&(current_stream_state -> operation_synchronizer));
   wake_up_interruptible(&(current_stream_state -> wq));

   return ret;
}


static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {
   
   int ret, priority, blocking, major, minor;
   object_state *current_stream_state;
   session *session;
   
   minor = get_minor(filp);
   major = get_major(filp);
   
   session = filp -> private_data;
   priority = session -> priority;
   blocking = session -> blocking;

   current_stream_state = &objects[minor][priority];

   AUDIT printk("%s current thread has called a read on %s device [MAJOR: %d, minor: %d]",
         MODNAME, DEVICE_NAME ,major, minor);

   if (unlikely(len == 0))
            return 0;

   if (blocking == BLOCKING) {

      AUDIT printk("%s current thread is waiting for bytes to read from device %s [MAJOR: %d, minor: %d]",
            MODNAME, DEVICE_NAME , major, minor);

      inc_pending_threads(minor,priority);
      ret = wait_event_interruptible_timeout(
                     current_stream_state -> wq,
                     check_if_readable_and_try_lock(current_stream_state),
                     msecs_to_jiffies(session -> timeout)
            );
      dec_pending_threads(minor,priority);

      AUDIT printk("%s current thread has woken up from wait queue related to device %s [MAJOR: %d, minor: %d]",
         MODNAME, DEVICE_NAME , major, minor);

      if(ret == 0) {
         AUDIT printk("%s timer has expired for current thread and it is not possible to read from device %s [MAJOR: %d, minor: %d]",
            MODNAME, DEVICE_NAME, major, minor);

         return -ETIME;
      } else if(ret == -ERESTARTSYS) {
         AUDIT printk("%s current thread was hit with a signal while waiting for bytes to read on device %s [MAJOR: %d, minor: %d]",
            MODNAME, DEVICE_NAME, major, minor);

         return -EINTR;
      }
   } else {
      if (!mutex_trylock( &(current_stream_state -> operation_synchronizer) ))
         return -EBUSY;
   }

   ret = read(current_stream_state, buff, len);

   mutex_unlock(&(current_stream_state->operation_synchronizer));
   wake_up_interruptible(&(current_stream_state->wq));

   return ret;
}



static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param) {

   session *session;
   session = filp->private_data;

   switch (command)
   {
   case 3:
      session->priority = LOW_PRIORITY;
      AUDIT printk("%s: somebody has set priority level to LOW on dev with " \
      "[major,minor] number [%d,%d] and command %u \n", MODNAME, get_major(filp), get_minor(filp), command);
      break;
   case 4:
      session->priority = HIGH_PRIORITY;
      AUDIT printk("%s: somebody has set priority level to HIGH on dev with " \
      "[major,minor] number [%d,%d] and command %u \n", MODNAME, get_major(filp), get_minor(filp), command);
      break;
   case 5:
      session->blocking = BLOCKING;
      AUDIT printk("%s: somebody has set BLOCKING r/w op on dev with " \
      "[major,minor] number [%d,%d] and command %u \n", MODNAME, get_major(filp), get_minor(filp), command);
      break;
   case 6:
      session->blocking = NON_BLOCKING;
      AUDIT printk("%s: somebody has set NON-BLOCKING r/w on dev with " \
      "[major,minor] number [%d,%d] and command %u \n", MODNAME, get_major(filp), get_minor(filp), command);
      break;
   case 7:
      session->timeout = param;
      AUDIT printk("%s: somebody has set TIMEOUT on dev with " \
      "[major,minor] number [%d,%d] and command %u \n", MODNAME, get_major(filp), get_minor(filp), command);
      break;
   default:
      AUDIT printk("%s: somebody called an invalid setting on dev with " \
      "[major,minor] number [%d,%d] and command %u \n", MODNAME, get_major(filp), get_minor(filp), command);
   }
   return 0;
}



static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = dev_write,
    .read = dev_read,
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = dev_ioctl
};




int init_module(void) {

   int i, j;
   // initialize the driver internal state
   for (i = 0; i < MINORS; i++)
   {

      for (j = 0; j < DATA_FLOWS; j++)
      {

         mutex_init(&(objects[i][j].operation_synchronizer));

         objects[i][j].head = kzalloc(sizeof(data_segment), GFP_KERNEL);
         objects[i][j].tail = kzalloc(sizeof(data_segment), GFP_KERNEL);
         if (objects[i][j].head == NULL || objects[i][j].tail == NULL)
         {
            printk("%s: unable to allocate a new data_segment\n", MODNAME);
            goto revert_allocation;
         }

         objects[i][j].head->next = (objects[i][j].tail);
         objects[i][j].head->previous = NULL;
         objects[i][j].head->buffer = NULL;

         objects[i][j].tail->next = NULL;
         objects[i][j].tail->previous = (objects[i][j].head);
         objects[i][j].tail->buffer = NULL;

         init_waitqueue_head(&objects[i][j].wq);

      }
   }

   Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
   // actually allowed minors are directly controlled within this driver

   if (Major < 0)
   {
      printk("%s: registering device failed\n", MODNAME);
      return Major;
   }

   AUDIT printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n", MODNAME, Major);

   return 0;

revert_allocation:
   for (; i >= 0; i--)
   {
      for (; j >= 0; j--)
      {
         kfree(objects[i][j].head);
         kfree(objects[i][j].tail);
      }
   }
   return -ENOMEM;
}



void cleanup_module(void) {

   int i, j;
   object_state *node;
   data_segment *head, *current_segment;

   for (i = 0; i < MINORS; i++)
   {
      for (j = 0; j < DATA_FLOWS; j++)
      {
         node = &(objects[i][j]);
         head = node -> head;
         while ( (head -> next) != (node -> tail)) {
            current_segment = head -> next;
            head->next = head->next->next;
            kfree(current_segment->buffer);
            kfree(current_segment);
         }
      }
   }

   unregister_chrdev(Major, DEVICE_NAME);

   AUDIT printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n", MODNAME, Major);

   return;
}
