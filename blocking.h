#include "info.h"

static int timeout_block(unsigned long, struct mutex *, wait_queue_head_t *);
static wait_queue_head_t * get_lock(object_state *, session *, int);

#ifndef _COMMONH_
#define _COMMONH_

static int timeout_block(unsigned long timeout, struct mutex *mutex, wait_queue_head_t *wq)
{
   int val;

   if (timeout == 0) return 0;

   AUDIT printk("%s: thread with pid %d will sleep for %lu millis\n", MODNAME, current->pid, timeout);

   timeout = msecs_to_jiffies(timeout); 

   val = wait_event_timeout(*wq, mutex_trylock(mutex), timeout);
   
   AUDIT printk("%s: thread %d got up from sleeping.\n", MODNAME, current->pid);
   
   if(!val) return 0;

   return 1;
}

static wait_queue_head_t * get_lock(object_state *the_object, session *session, int minor){

   int ret;
   wait_queue_head_t *wq;
   wq = &the_object->wq;
   
   ret = mutex_trylock(&(the_object->operation_synchronizer));
   if (!ret)
   {

      AUDIT printk("%s: Unable to get lock now.\n", MODNAME);
      if (session->blocking == BLOCKING)
      {

         if(session->priority == HIGH_PRIORITY) hp_threads[minor] ++;
         else lp_threads[minor] ++;

         ret = timeout_block(session->timeout, &the_object->operation_synchronizer, wq);
         
         if(session->priority == HIGH_PRIORITY) hp_threads[minor] --;
         else lp_threads[minor] --;

         if (ret == 0) return NULL;
      }
      else
         return NULL;
   }

   return wq;
}

#endif
