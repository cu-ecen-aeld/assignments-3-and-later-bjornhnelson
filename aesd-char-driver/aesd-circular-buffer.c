/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#include <linux/slab.h>
#else
#include <string.h>
#include <stdlib.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer. 
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
			size_t char_offset, size_t *entry_offset_byte_rtn )
{

    int buf_pos = buffer->out_offs;
    int size_sum = 0;
    int cur_size;

    // empty case
    if (buffer->out_offs == buffer->in_offs && buffer->full == false) {
        return NULL;
    }

    do {
        cur_size = buffer->entry[buf_pos].size;

        // desired offset in current entry
        if (char_offset < size_sum + cur_size) {
            *entry_offset_byte_rtn = char_offset - size_sum;
            return &(buffer->entry[buf_pos]);
        }

        // go to next entry
        size_sum += cur_size;
        buf_pos++;
        buf_pos %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    } while (buf_pos != buffer->in_offs);

    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
const char* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    const char* overwrite_ptr = NULL;

    // update read pos
    if (buffer->full) {
        overwrite_ptr = buffer->entry[buffer->out_offs].buffptr;
        buffer->out_offs++;
        buffer->out_offs %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    buffer->entry[buffer->in_offs] = *add_entry; // add entry
    
    // update write pos
    buffer->in_offs++;
    buffer->in_offs %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    // check if buffer is full
    if (buffer->in_offs == buffer->out_offs) {
        buffer->full = true;
    }

    return overwrite_ptr;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}

extern void aesd_circular_buffer_free(struct aesd_circular_buffer *buffer) {

    int buf_pos = buffer->out_offs;

    // empty case
    if (buffer->out_offs == buffer->in_offs && buffer->full == false) {
        return;
    }

    do {
        #ifdef __KERNEL__
            kfree(buffer->entry[buf_pos].buffptr);
        #else
            free((char *)buffer->entry[buf_pos].buffptr);
        #endif
        buffer->entry[buf_pos].size = 0;
        buf_pos++;
        buf_pos %= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    } while (buf_pos != buffer->in_offs);

    buffer->in_offs = 0;
    buffer->out_offs = 0;
}