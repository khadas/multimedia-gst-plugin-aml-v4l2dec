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

#ifndef __GST_AML_V4L2_VIDEO_DEC_H__
#define __GST_AML_V4L2_VIDEO_DEC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/video/gstvideometa.h>

#include <gstamlv4l2object.h>
#include <gstamlv4l2bufferpool.h>

G_BEGIN_DECLS

#define GST_TYPE_AML_V4L2_VIDEO_DEC \
    (gst_aml_v4l2_video_dec_get_type())
#define GST_AML_V4L2_VIDEO_DEC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AML_V4L2_VIDEO_DEC, GstAmlV4l2VideoDec))
#define GST_AML_V4L2_VIDEO_DEC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AML_V4L2_VIDEO_DEC, GstAmlV4l2VideoDecClass))
#define GST_IS_AML_V4L2_VIDEO_DEC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AML_V4L2_VIDEO_DEC))
#define GST_IS_AML_V4L2_VIDEO_DEC_CLASS(obj) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AML_V4L2_VIDEO_DEC))

typedef struct _GstAmlV4l2VideoDec GstAmlV4l2VideoDec;
typedef struct _GstAmlV4l2VideoDecClass GstAmlV4l2VideoDecClass;

struct _GstAmlV4l2VideoDec
{
    GstVideoDecoder parent;

    /* < private > */
    GstAmlV4l2Object *v4l2output;
    GstAmlV4l2Object *v4l2capture;

    /* pads */
    GstCaps *probed_srccaps;
    GstCaps *probed_sinkcaps;

    /* State */
    GstVideoCodecState *input_state;
    gboolean active;
    GstFlowReturn output_flow;

    /* flags */
    gboolean is_secure_path;
};

struct _GstAmlV4l2VideoDecClass
{
    GstVideoDecoderClass parent_class;

    gchar *default_device;
};

GType gst_aml_v4l2_video_dec_get_type(void);

gboolean gst_aml_v4l2_is_video_dec(GstCaps *sink_caps, GstCaps *src_caps);
void gst_aml_v4l2_video_dec_register(GstPlugin *plugin,
                                     const gchar *basename,
                                     const gchar *device_path,
                                     GstCaps *sink_caps, GstCaps *src_caps);

G_END_DECLS

#endif /* __GST_AML_V4L2_VIDEO_DEC_H__ */
