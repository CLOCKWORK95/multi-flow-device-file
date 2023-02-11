#include "info.h"

int read(object_state *, char __user *, size_t);


int read(object_state *current_stream_state, char __user *buff, size_t len) {

   int res;
   size_t read_bytes, current_readable_bytes, current_read_len;
   data_segment *current_segment, *head;

   if (unlikely(current_stream_state -> valid_bytes == 0))
            return -EAGAIN;

   read_bytes = 0;

   head = current_stream_state -> head;

   while ((head -> next != current_stream_state -> tail) && (len > read_bytes)) {
      
      current_segment = head -> next;
      current_readable_bytes = current_segment -> actual_size - current_segment -> off;
      current_read_len = MIN(len - read_bytes, current_readable_bytes);

      res = copy_to_user(buff + read_bytes, &(current_segment -> buffer[current_segment -> off]), current_read_len);

      read_bytes += ( current_read_len - res );
      current_segment -> off += ( current_read_len - res );

      if (current_segment -> off == current_segment -> actual_size) {
               head->next = head->next->next;
               head->next->previous = head;

               kfree(current_segment -> buffer);
               kfree(current_segment);
      }

      if (unlikely(res != 0))
               break;
   }

   current_stream_state->valid_bytes -= read_bytes;

   return read_bytes;
}