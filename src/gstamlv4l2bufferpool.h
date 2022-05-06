/* GStreamer
 * Copyright (C) 2022 <xuesong.jiang@amlogic.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef __GST_AML_V4L2_BUFFER_POOL_H__
#define __GST_AML_V4L2_BUFFER_POOL_H__

#include <gst/gst.h>

typedef struct _GstAmlV4l2BufferPool GstAmlV4l2BufferPool;
typedef struct _GstAmlV4l2BufferPoolClass GstAmlV4l2BufferPoolClass;
typedef struct _GstAmlV4l2Meta GstAmlV4l2Meta;

#include "gstamlv4l2object.h"
#include "gstamlv4l2allocator.h"

G_BEGIN_DECLS

#define GST_TYPE_AML_V4L2_BUFFER_POOL (gst_aml_v4l2_buffer_pool_get_type())
#define GST_IS_AML_V4L2_BUFFER_POOL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AML_V4L2_BUFFER_POOL))
#define GST_AML_V4L2_BUFFER_POOL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AML_V4L2_BUFFER_POOL, GstAmlV4l2BufferPool))
#define GST_AML_V4L2_BUFFER_POOL_CAST(obj) ((GstAmlV4l2BufferPool *)(obj))

/* This flow return is used to indicated that the last buffer of a
 * drain or a resoltuion change has been found. This should normally
 * only occure for mem-2-mem devices. */
#define GST_AML_V4L2_FLOW_LAST_BUFFER GST_FLOW_CUSTOM_SUCCESS

/* This flow return is used to indicated that the returned buffer was marked
 * with the error flag and had no payload. This error should be recovered by
 * simply waiting for next buffer. */
#define GST_AML_V4L2_FLOW_CORRUPTED_BUFFER GST_FLOW_CUSTOM_SUCCESS_1
/* This flow return is used to indicated that the last buffer of a
 * resoltuion change has been found. This should normally only
 * occure for mem-2-mem devices. */
#define GST_AML_V4L2_FLOW_SOURCE_CHANGE GST_FLOW_CUSTOM_SUCCESS_2

#define GST_AML_SPEC_FLOW_FOR_VBP 1

struct _GstAmlV4l2BufferPool
{
    GstBufferPool parent;

    GstAmlV4l2Object *obj; /* the v4l2 object */
    gint video_fd;         /* a dup(2) of the v4l2object's video_fd */
    GstPoll *poll;         /* a poll for video_fd */
    GstPollFD pollfd;
    gboolean can_poll_device;

    gboolean empty;
    GCond empty_cond;

    gboolean orphaned;

    GstAmlV4l2Allocator *vallocator;
    GstAllocator *allocator;
    GstAllocationParams params;
    GstBufferPool *other_pool;
    guint size;
    GstVideoInfo caps_info; /* Default video information */

    gboolean add_videometa;         /* set if video meta should be added */
    gboolean enable_copy_threshold; /* If copy_threshold should be set */

    guint min_latency;    /* number of buffers we will hold */
    guint max_latency;    /* number of buffers we can hold */
    guint num_queued;     /* number of buffers queued in the driver */
    guint num_allocated;  /* number of buffers allocated */
    guint copy_threshold; /* when our pool runs lower, start handing out copies */

    gboolean streaming;
    gboolean flushing;

    GstBuffer *buffers[VIDEO_MAX_FRAME];
#ifdef GST_AML_SPEC_FLOW_FOR_VBP
    GstBuffer *read_to_free_bufs[VIDEO_MAX_FRAME];
    gint ready_to_free_buf_num;
#endif

    /* signal handlers */
    gulong group_released_handler;

    /* Control to warn only once on buggy feild driver bug */
    gboolean has_warned_on_buggy_field;
};

struct _GstAmlV4l2BufferPoolClass
{
    GstBufferPoolClass parent_class;
};

GType gst_aml_v4l2_buffer_pool_get_type(void);

GstBufferPool *gst_aml_v4l2_buffer_pool_new(GstAmlV4l2Object *obj, GstCaps *caps);

GstFlowReturn gst_aml_v4l2_buffer_pool_process(GstAmlV4l2BufferPool *bpool, GstBuffer **buf);

void gst_aml_v4l2_buffer_pool_set_other_pool(GstAmlV4l2BufferPool *pool,
                                             GstBufferPool *other_pool);
void gst_aml_v4l2_buffer_pool_copy_at_threshold(GstAmlV4l2BufferPool *pool,
                                                gboolean copy);

gboolean gst_aml_v4l2_buffer_pool_flush(GstBufferPool *pool);

gboolean gst_aml_v4l2_buffer_pool_orphan(GstBufferPool **pool);

void gst_aml_v4l2_buffer_pool_dump_stat(GstAmlV4l2BufferPool *pool, const gchar *file_name, gint try_num);

G_END_DECLS

#endif /*__GST_AML_V4L2_BUFFER_POOL_H__ */
