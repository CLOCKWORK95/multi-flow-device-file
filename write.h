#include "blocking.h"
#include "info.h"

int write(object_state *, const char *, loff_t *, size_t, session *, int);
void deferred_write(unsigned long);
long put_work(char *, size_t, loff_t *, session *, int);



typedef struct _packed_work
{
        void *buffer;
        struct work_struct the_work;
        const char *data;
        size_t len;
        loff_t *off;
        session *session;
        int minor;

} packed_work;



int write(object_state *the_object,
          const char *buff,
          loff_t *off,
          size_t len,
          session *session,
          int minor)
{
        int ret;
        char* new_buffer, * new_content;
        wait_queue_head_t *wq;



        if( *off >= OBJECT_MAX_SIZE ) { //offset too large
                return -ENOSPC;  //no space left on device
        } 
        if( *off > the_object -> valid_bytes ) { //offset beyond the current stream size
                return -ENOSR;  //out of stream resources
        } 
        if( ( OBJECT_MAX_SIZE - *off ) < len ) len = OBJECT_MAX_SIZE - *off;

        new_buffer = kzalloc( OBJECT_MAX_SIZE, GFP_ATOMIC );
        new_content = kzalloc( len, GFP_ATOMIC );
        if( new_buffer == NULL || new_content == NULL ){
                AUDIT printk("%s: unable to reserve new memory buffers.\n", MODNAME);
                module_put(THIS_MODULE);
                return -ENOMEM;
        }

        ret = copy_from_user( new_content, buff, len );
        strncpy( new_buffer, the_object -> buffer , strlen(the_object -> buffer) );
        strncat( new_buffer, new_content, len );
        AUDIT printk( "%s: new buffer content:  %s has been written", MODNAME, new_buffer );
        kfree(new_content);

        wq = get_lock(the_object, session, minor);
        if (wq == NULL)
                return -EAGAIN;

        if( session -> priority == HIGH_PRIORITY ) hp_bytes[minor] += len;
        else lp_bytes[minor] += len;

        kfree(the_object -> buffer);
        *off += (len - ret);
        the_object -> buffer = new_buffer;
        the_object -> valid_bytes = *off;

#ifndef TEST_BLOCKING_OPS  
        mutex_unlock(&(the_object->operation_synchronizer));
        wake_up(wq);
#endif
        return ret;
}



void deferred_write(unsigned long data)
{
        session *session = container_of((void *)data, packed_work, the_work)->session;
        int minor = container_of((void *)data, packed_work, the_work)->minor;
        size_t len = container_of((void *)data, packed_work, the_work)->len;
        loff_t *off = container_of((void *)data, packed_work, the_work)->off;
        
        object_state *the_object = objects[minor];

        char *buff = kzalloc(len, GFP_ATOMIC); // non blocking memory allocation
        if (buff == NULL)
        {
                AUDIT printk("%s: work buffer allocation failure\n", MODNAME);
                goto exit;
        }

        buff = (char *)container_of((void *)data, packed_work, the_work)->data;

        AUDIT printk("%s: this print comes from kworker daemon with PID=%d - running on CPU-core %d\n", MODNAME, current->pid, smp_processor_id());

        write(the_object, buff, off, len, session, minor);

        AUDIT printk("%s: releasing the work buffer at address %p - container of work is at %p\n", MODNAME, (void *)data, container_of((void *)data, packed_work, the_work));

        kfree(buff);

exit:

        kfree((void *)container_of((void *)data, packed_work, the_work));
        module_put(THIS_MODULE);
}




long put_work(char *buff,
              size_t len,
              loff_t *off,
              session *session,
              int minor)
{

        packed_work *the_work;

        if (!try_module_get(THIS_MODULE))
                return -ENODEV;

        AUDIT printk("%s: requested deferred work\n", MODNAME);

        the_work = kzalloc(sizeof(packed_work), GFP_ATOMIC); // non blocking memory allocation
        if (the_work == NULL)
        {
                AUDIT printk("%s: work buffer allocation failure\n", MODNAME);
                module_put(THIS_MODULE);
                return -ENOMEM;
        }

        the_work->buffer = the_work;
        the_work->len = len;
        the_work->off = off;
        the_work->session = session;
        the_work->minor = minor;

        the_work->data = kzalloc(len, GFP_ATOMIC); // non blocking memory allocation
        if ( the_work -> data == NULL )
        {
                AUDIT printk("%s: work buffer allocation failure\n", MODNAME);
                module_put(THIS_MODULE);
                return -ENOMEM;
        }

        strncpy((char *)the_work->data, buff, len);

        AUDIT printk("%s: work buffer allocation success - address is %p\n", MODNAME, the_work);

        __INIT_WORK(&(the_work->the_work), (void *)deferred_write, (unsigned long)(&(the_work->the_work)));

        schedule_work(&the_work->the_work);

        return 0;
}