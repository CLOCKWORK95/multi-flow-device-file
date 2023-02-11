#include "info.h"

size_t write(data_segment *, object_state *);
void deferred_write(unsigned long);
int put_work(object_state *, data_segment *, int, int, gfp_t);


typedef struct _packed_work
{
        int major;
        int minor;
        object_state *the_stream_state;
        data_segment *new_segment;
        struct work_struct  the_work;

} packed_work;


size_t write( data_segment *new_segment, object_state *current_stream_state ) {
        data_segment *tail;
        
        tail =  current_stream_state -> tail;

        new_segment -> previous = tail->previous;
        new_segment -> next = tail;

        tail->previous -> next = new_segment;
        tail->previous = new_segment;

        current_stream_state -> valid_bytes += new_segment -> actual_size;

        return new_segment -> actual_size;
}


void deferred_write(unsigned long data) {
        packed_work *the_task = (packed_work *) container_of( (void*) data, packed_work, the_work );
        AUDIT printk("%s kworker %d handles async write operation on device [MAJOR: %d, minor: %d]",
               MODNAME, current->pid, the_task->major, the_task->minor);

        mutex_lock( &( the_task->the_stream_state->operation_synchronizer) );
        the_task->the_stream_state->pending_bytes -= write(the_task->new_segment, the_task->the_stream_state);
        mutex_unlock( &( the_task->the_stream_state->operation_synchronizer) );

        wake_up_interruptible(&(the_task->the_stream_state->wq));

        kfree(the_task);

        module_put(THIS_MODULE);
}


int put_work( object_state *current_stream_state, data_segment *new_segment, int major, int minor, gfp_t flags ) {
        
        packed_work *the_task;
        int ret;

        if(!try_module_get(THIS_MODULE))
                return -ENODEV;

        the_task = (packed_work *)kzalloc(sizeof(packed_work), flags);
        if(unlikely(the_task == NULL))
                return -ENOMEM;

        the_task -> major = major;
        the_task -> minor = minor;
        the_task -> the_stream_state = current_stream_state;
        the_task -> new_segment = new_segment;

        ret = new_segment -> actual_size;

        __INIT_WORK(&(the_task -> the_work), (void*) deferred_write, (unsigned long)(&(the_task -> the_work)));
        schedule_work( &the_task -> the_work );

        current_stream_state -> pending_bytes += ret;

        return ret;
}