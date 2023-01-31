#include "blocking.h"
#include "info.h"

int get_min( int a, int b );
int read(object_state *, char *, loff_t *, size_t, session *, int);



int get_min( int a, int b ) {
   if ( a <= b ) return a;
   return b;
}



int read(object_state *the_object,
         char *buff,
         loff_t *off,
         size_t len,
         session *session,
         int minor) {

   int ret = 0, buff_length = 0, bytes_to_read = 0 , diff = 0;

   char* remaining_buff_content;

   wait_queue_head_t *wq;

   wq = get_lock( the_object, session, minor );
   if (wq == NULL)
      return -EAGAIN;

   *off = 0;

   buff_length = strlen( the_object -> buffer );

   bytes_to_read = get_min( buff_length, (int) len );

   ret = copy_to_user( buff, &( the_object -> buffer[0] ), bytes_to_read );

   ret += bytes_to_read;

   if ( buff_length  > bytes_to_read ){ 
      // delete & shift (FIFO)
      diff = buff_length - bytes_to_read;
      remaining_buff_content = kzalloc( diff, GFP_ATOMIC );
      if ( remaining_buff_content == NULL )
        {
                AUDIT printk("%s: unable to allocate memory buffer.\n", MODNAME);
                return -ENOMEM;
        }
      strncpy( &remaining_buff_content[0], &( the_object -> buffer[ bytes_to_read ] ), diff );
      kfree( the_object -> buffer );
      the_object -> buffer = kzalloc( OBJECT_MAX_SIZE, GFP_ATOMIC );
      strncpy( the_object -> buffer, &remaining_buff_content[0], diff );
      kfree( remaining_buff_content );
   }
   else{
      // just delete
      kfree( the_object -> buffer );
      the_object -> buffer = kzalloc( OBJECT_MAX_SIZE, GFP_ATOMIC );
   }

   *off += diff;
   the_object -> valid_bytes = diff;

   if (session->priority == HIGH_PRIORITY)
      hp_bytes[minor] -= ret;
   else
      lp_bytes[minor] -= ret;

   mutex_unlock(&(the_object->operation_synchronizer));
   wake_up(wq);

   return ret;
}