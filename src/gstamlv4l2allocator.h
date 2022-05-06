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

#ifndef __GST_AML_V4L2_ALLOCATOR_H__
#define __GST_AML_V4L2_ALLOCATOR_H__

#include "ext/videodev2.h"
#include <gst/gst.h>
#include <gst/gstatomicqueue.h>

G_BEGIN_DECLS

#define GST_TYPE_AML_V4L2_ALLOCATOR (gst_aml_v4l2_allocator_get_type())
#define GST_IS_AML_V4L2_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AML_V4L2_ALLOCATOR))
#define GST_IS_AML_V4L2_ALLOCATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AML_V4L2_ALLOCATOR))
#define GST_AML_V4L2_ALLOCATOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_AML_V4L2_ALLOCATOR, GstAmlV4l2AllocatorClass))
#define GST_AML_V4L2_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AML_V4L2_ALLOCATOR, GstAmlV4l2Allocator))
#define GST_AML_V4L2_ALLOCATOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AML_V4L2_ALLOCATOR, GstAmlV4l2AllocatorClass))
#define GST_AML_V4L2_ALLOCATOR_CAST(obj) ((GstAmlV4l2Allocator *)(obj))

#define GST_AML_V4L2_ALLOCATOR_CAN_REQUEST(obj, type) \
    (GST_OBJECT_FLAG_IS_SET(obj, GST_V4L2_ALLOCATOR_FLAG_##type##_REQBUFS))
#define GST_AML_V4L2_ALLOCATOR_CAN_ALLOCATE(obj, type) \
    (GST_OBJECT_FLAG_IS_SET(obj, GST_V4L2_ALLOCATOR_FLAG_##type##_CREATE_BUFS))
#define GST_AML_V4L2_ALLOCATOR_CAN_ORPHAN_BUFS(obj) \
    (GST_OBJECT_FLAG_IS_SET(obj, GST_V4L2_ALLOCATOR_FLAG_SUPPORTS_ORPHANED_BUFS))
#define GST_AML_V4L2_ALLOCATOR_IS_ORPHANED(obj) \
    (GST_OBJECT_FLAG_IS_SET(obj, GST_V4L2_ALLOCATOR_FLAG_ORPHANED))

#define GST_AML_V4L2_MEMORY_QUARK gst_aml_v4l2_memory_quark()

typedef struct _GstAmlV4l2Allocator GstAmlV4l2Allocator;
typedef struct _GstAmlV4l2AllocatorClass GstAmlV4l2AllocatorClass;
typedef struct _GstAmlV4l2MemoryGroup GstAmlV4l2MemoryGroup;
typedef struct _GstAmlV4l2Memory GstAmlV4l2Memory;
typedef enum _GstAmlV4l2Capabilities GstAmlV4l2Capabilities;
typedef enum _GstAmlV4l2Return GstAmlV4l2Return;

enum _GstAmlV4l2AllocatorFlags
{
    GST_V4L2_ALLOCATOR_FLAG_MMAP_REQBUFS = (GST_ALLOCATOR_FLAG_LAST << 0),
    GST_V4L2_ALLOCATOR_FLAG_MMAP_CREATE_BUFS = (GST_ALLOCATOR_FLAG_LAST << 1),
    GST_V4L2_ALLOCATOR_FLAG_USERPTR_REQBUFS = (GST_ALLOCATOR_FLAG_LAST << 2),
    GST_V4L2_ALLOCATOR_FLAG_USERPTR_CREATE_BUFS = (GST_ALLOCATOR_FLAG_LAST << 3),
    GST_V4L2_ALLOCATOR_FLAG_DMABUF_REQBUFS = (GST_ALLOCATOR_FLAG_LAST << 4),
    GST_V4L2_ALLOCATOR_FLAG_DMABUF_CREATE_BUFS = (GST_ALLOCATOR_FLAG_LAST << 5),
    GST_V4L2_ALLOCATOR_FLAG_SUPPORTS_ORPHANED_BUFS = (GST_ALLOCATOR_FLAG_LAST << 6),
    GST_V4L2_ALLOCATOR_FLAG_ORPHANED = (GST_ALLOCATOR_FLAG_LAST << 7),
};

enum _GstAmlV4l2Return
{
    GST_V4L2_OK = 0,
    GST_AML_V4L2_ERROR = -1,
    GST_V4L2_BUSY = -2
};

struct _GstAmlV4l2Memory
{
    GstMemory mem;
    gint plane;
    GstAmlV4l2MemoryGroup *group;
    gpointer data;
    gint dmafd;
};

struct _GstAmlV4l2MemoryGroup
{
    gint n_mem;
    GstMemory *mem[VIDEO_MAX_PLANES];
    gint mems_allocated;
    struct v4l2_buffer buffer;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
};

struct _GstAmlV4l2Allocator
{
    GstAllocator parent;
    GstAmlV4l2Object *obj;
    guint32 count;
    guint32 memory;
    gboolean can_allocate;
    gboolean active;

    GstAmlV4l2MemoryGroup *groups[VIDEO_MAX_FRAME];
    GstAtomicQueue *free_queue;
    GstAtomicQueue *pending_queue;
};

struct _GstAmlV4l2AllocatorClass
{
    GstAllocatorClass parent_class;
};

GType gst_aml_v4l2_allocator_get_type(void);

gboolean gst_is_aml_v4l2_memory(GstMemory *mem);

GQuark gst_aml_v4l2_memory_quark(void);

gboolean gst_aml_v4l2_allocator_is_active(GstAmlV4l2Allocator *allocator);

guint gst_aml_v4l2_allocator_get_size(GstAmlV4l2Allocator *allocator);

GstAmlV4l2Allocator *gst_aml_v4l2_allocator_new(GstObject *parent, GstAmlV4l2Object *obj);

guint gst_aml_v4l2_allocator_start(GstAmlV4l2Allocator *allocator,
                                   guint32 count, guint32 memory);

GstAmlV4l2Return gst_aml_v4l2_allocator_stop(GstAmlV4l2Allocator *allocator);

gboolean gst_aml_v4l2_allocator_orphan(GstAmlV4l2Allocator *allocator);

GstAmlV4l2MemoryGroup *gst_aml_v4l2_allocator_alloc_mmap(GstAmlV4l2Allocator *allocator);

GstAmlV4l2MemoryGroup *gst_aml_v4l2_allocator_alloc_dmabuf(GstAmlV4l2Allocator *allocator,
                                                           GstAllocator *dmabuf_allocator);

GstAmlV4l2MemoryGroup *gst_aml_v4l2_allocator_alloc_dmabufin(GstAmlV4l2Allocator *allocator);

GstAmlV4l2MemoryGroup *gst_aml_v4l2_allocator_alloc_userptr(GstAmlV4l2Allocator *allocator);

gboolean gst_aml_v4l2_allocator_import_dmabuf(GstAmlV4l2Allocator *allocator,
                                              GstAmlV4l2MemoryGroup *group,
                                              gint n_mem, GstMemory **dma_mem);

gboolean gst_aml_v4l2_allocator_import_userptr(GstAmlV4l2Allocator *allocator,
                                               GstAmlV4l2MemoryGroup *group,
                                               gsize img_size, int n_planes,
                                               gpointer *data, gsize *size);

void gst_aml_v4l2_allocator_flush(GstAmlV4l2Allocator *allocator);

gboolean gst_aml_v4l2_allocator_qbuf(GstAmlV4l2Allocator *allocator,
                                     GstAmlV4l2MemoryGroup *group);

GstFlowReturn gst_aml_v4l2_allocator_dqbuf(GstAmlV4l2Allocator *allocator,
                                           GstAmlV4l2MemoryGroup **group);

void gst_aml_v4l2_allocator_reset_group(GstAmlV4l2Allocator *allocator,
                                        GstAmlV4l2MemoryGroup *group);

G_END_DECLS

#endif /* __GST_AML_V4L2_ALLOCATOR_H__ */
