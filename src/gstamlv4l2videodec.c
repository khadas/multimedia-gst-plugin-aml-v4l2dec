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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "gstamlv4l2object.h"
#include "gstamlv4l2videodec.h"

#include <string.h>
#include <gst/gst-i18n-plugin.h>
#include <gst/allocators/gstdmabuf.h>

GST_DEBUG_CATEGORY_STATIC(gst_aml_v4l2_video_dec_debug);
#define GST_CAT_DEFAULT gst_aml_v4l2_video_dec_debug

#ifdef GST_VIDEO_DECODER_STREAM_LOCK
#undef GST_VIDEO_DECODER_STREAM_LOCK
#define GST_VIDEO_DECODER_STREAM_LOCK(decoder)                      \
    {                                                               \
        GST_TRACE("aml v4l2 dec locking");                           \
        g_rec_mutex_lock(&GST_VIDEO_DECODER(decoder)->stream_lock); \
        GST_TRACE("aml v4l2 dec locked");                            \
    }
#endif

#ifdef GST_VIDEO_DECODER_STREAM_UNLOCK
#undef GST_VIDEO_DECODER_STREAM_UNLOCK
#define GST_VIDEO_DECODER_STREAM_UNLOCK(decoder)                      \
    {                                                                 \
        GST_TRACE("aml v4l2 dec unlocking");                           \
        g_rec_mutex_unlock(&GST_VIDEO_DECODER(decoder)->stream_lock); \
        GST_TRACE("aml v4l2 dec unlocked");                            \
    }
#endif

#ifndef ABSDIFF
#define ABSDIFF(a,b) (((a) > (b)) ? ((a) - (b)) : ((b) - (a)))
#endif

#if GST_IMPORT_LGE_PROP
typedef struct _GstAmlResourceInfo
{
    gchar *coretype;
    gint videoport;
    gint audioport;
    gint maxwidth;
    gint maxheight;
    gint mixerport;
} GstAmlResourceInfo;

struct _GstAmlV4l2VideoDecLgeCtxt
{
    GstAmlResourceInfo res_info;
    guint64 dec_size;
    guint64 undec_size;
    gchar *app_type;
    gboolean clip_mode;
};
#endif

typedef struct
{
    gchar *device;
    GstCaps *sink_caps;
    GstCaps *src_caps;
    const gchar *longname;
    const gchar *description;
} GstAmlV4l2VideoDecCData;

enum
{
    PROP_0,
    V4L2_STD_OBJECT_PROPS,
#if GST_IMPORT_LGE_PROP
    LGE_RESOURCE_INFO,
    LGE_DECODE_SIZE,
    LGE_UNDECODE_SIZE,
    LGE_APP_TYPE,
    LGE_CLIP_MODE
#endif
};

enum
{
  SIGNAL_DECODED_PTS,
  MAX_SIGNAL
};

static guint g_signals[MAX_SIGNAL]= {0};

#define gst_aml_v4l2_video_dec_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE(GstAmlV4l2VideoDec, gst_aml_v4l2_video_dec,
                       GST_TYPE_VIDEO_DECODER);

static GstFlowReturn gst_aml_v4l2_video_dec_finish(GstVideoDecoder *decoder);
#if GST_IMPORT_LGE_PROP
static void gst_aml_v4l2_video_dec_install_lge_properties_helper(GObjectClass *gobject_class);
#endif

static void
gst_aml_v4l2_video_dec_set_property(GObject *object,
                                    guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(object);

    switch (prop_id)
    {
    case PROP_CAPTURE_IO_MODE:
        if (!gst_aml_v4l2_object_set_property_helper(self->v4l2capture,
                                                     prop_id, value, pspec))
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
        break;
    case PROP_DUMP_FRAME_LOCATION:
        if (!gst_aml_v4l2_object_set_property_helper(self->v4l2capture,
                                                     prop_id, value, pspec))
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
        break;
#if GST_IMPORT_LGE_PROP
    case LGE_RESOURCE_INFO:
    {
        GST_DEBUG_OBJECT(self, "LGE up layer set res info");
        GstStructure *r_info = g_value_get_object(value);
        if (r_info)
        {
            if (gst_structure_has_field(r_info, "coretype"))
            {
                if (self->lge_ctxt->res_info.coretype)
                    g_free(self->lge_ctxt->res_info.coretype);
                self->lge_ctxt->res_info.coretype = g_strdup(gst_structure_get_string(r_info, "coretype"));
            }
            if (gst_structure_has_field(r_info, "videoport"))
                gst_structure_get_int(r_info, "videoport", &(self->lge_ctxt->res_info.videoport));
            if (gst_structure_has_field(r_info, "audioport"))
                gst_structure_get_int(r_info, "audioport", &(self->lge_ctxt->res_info.audioport));
            if (gst_structure_has_field(r_info, "maxwidth"))
                gst_structure_get_int(r_info, "maxwidth", &(self->lge_ctxt->res_info.maxwidth));
            if (gst_structure_has_field(r_info, "maxheight"))
                gst_structure_get_int(r_info, "maxheight", &(self->lge_ctxt->res_info.maxheight));
            if (gst_structure_has_field(r_info, "mixerport"))
                gst_structure_get_int(r_info, "mixerport", &(self->lge_ctxt->res_info.mixerport));
        }
        break;
    }
    case LGE_APP_TYPE:
    {
        GST_DEBUG_OBJECT(self, "LGE up layer set app type");
        if (self->lge_ctxt->app_type)
            g_free(self->lge_ctxt->app_type);
        self->lge_ctxt->app_type = g_strdup(g_value_get_string(value));
        break;
    }
    case LGE_CLIP_MODE:
    {
        GST_DEBUG_OBJECT(self, "LGE up layer set clip mode");
        self->lge_ctxt->clip_mode = g_strdup(g_value_get_boolean(value));
        break;
    }
#endif
        /* By default, only set on output */
    default:
        if (!gst_aml_v4l2_object_set_property_helper(self->v4l2output,
                                                     prop_id, value, pspec))
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
        break;
    }
}

static void
gst_aml_v4l2_video_dec_get_property(GObject *object,
                                    guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(object);

    switch (prop_id)
    {
    case PROP_CAPTURE_IO_MODE:
        if (!gst_aml_v4l2_object_get_property_helper(self->v4l2capture,
                                                     prop_id, value, pspec))
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
        break;

#if GST_IMPORT_LGE_PROP
    case LGE_DECODE_SIZE:
    {
        GST_DEBUG_OBJECT(self, "LGE up layer get dec size");
        self->lge_ctxt->dec_size = -1;
        g_value_set_int(value, self->lge_ctxt->dec_size);
        break;
    }
    case LGE_UNDECODE_SIZE:
    {
        GST_DEBUG_OBJECT(self, "LGE up layer get undec size");
        self->lge_ctxt->undec_size = -1;
        g_value_set_int(value, self->lge_ctxt->undec_size);
        break;
    }
#endif

        /* By default read from output */
    default:
        if (!gst_aml_v4l2_object_get_property_helper(self->v4l2output,
                                                     prop_id, value, pspec))
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        }
        break;
    }
}

static gboolean
gst_aml_v4l2_video_dec_open(GstVideoDecoder *decoder)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);
    GstCaps *codec_caps;

    GST_DEBUG_OBJECT(self, "Opening");

    if (!gst_aml_v4l2_object_open(self->v4l2output))
        goto failure;

    if (!gst_aml_v4l2_object_open_shared(self->v4l2capture, self->v4l2output))
        goto failure;

    codec_caps = gst_pad_get_pad_template_caps(decoder->sinkpad);
    self->probed_sinkcaps = gst_aml_v4l2_object_probe_caps(self->v4l2output,
                                                           codec_caps);
    gst_caps_unref(codec_caps);

    if (gst_caps_is_empty(self->probed_sinkcaps))
        goto no_encoded_format;

    return TRUE;

no_encoded_format:
    GST_ELEMENT_ERROR(self, RESOURCE, SETTINGS,
                      (_("Decoder on device %s has no supported input format"),
                       self->v4l2output->videodev),
                      (NULL));
    goto failure;

failure:
    if (GST_AML_V4L2_IS_OPEN(self->v4l2output))
        gst_aml_v4l2_object_close(self->v4l2output);

    if (GST_AML_V4L2_IS_OPEN(self->v4l2capture))
        gst_aml_v4l2_object_close(self->v4l2capture);

    gst_caps_replace(&self->probed_srccaps, NULL);
    gst_caps_replace(&self->probed_sinkcaps, NULL);

    return FALSE;
}

static gboolean
gst_aml_v4l2_video_dec_close(GstVideoDecoder *decoder)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);

    GST_DEBUG_OBJECT(self, "Closing");

    gst_aml_v4l2_object_close(self->v4l2output);
    gst_aml_v4l2_object_close(self->v4l2capture);
    gst_caps_replace(&self->probed_srccaps, NULL);
    gst_caps_replace(&self->probed_sinkcaps, NULL);

    return TRUE;
}

static gboolean
gst_aml_v4l2_video_dec_start(GstVideoDecoder *decoder)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);

    GST_DEBUG_OBJECT(self, "Starting");

    gst_aml_v4l2_object_unlock(self->v4l2output);
    g_atomic_int_set(&self->active, TRUE);
    self->output_flow = GST_FLOW_OK;

    return TRUE;
}

static gboolean
gst_aml_v4l2_video_dec_stop(GstVideoDecoder *decoder)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);

    GST_DEBUG_OBJECT(self, "Stopping");

    gst_aml_v4l2_object_unlock(self->v4l2output);
    gst_aml_v4l2_object_unlock(self->v4l2capture);

    /* Wait for capture thread to stop */
    gst_pad_stop_task(decoder->srcpad);

    GST_VIDEO_DECODER_STREAM_LOCK(decoder);
    self->output_flow = GST_FLOW_OK;
    GST_VIDEO_DECODER_STREAM_UNLOCK(decoder);

    /* Should have been flushed already */
    g_assert(g_atomic_int_get(&self->active) == FALSE);

    gst_aml_v4l2_object_stop(self->v4l2output);
    gst_aml_v4l2_object_stop(self->v4l2capture);

    if (self->input_state)
    {
        gst_video_codec_state_unref(self->input_state);
        self->input_state = NULL;
    }

    GST_DEBUG_OBJECT(self, "Stopped");

    return TRUE;
}

static gboolean
gst_aml_v4l2_video_dec_set_format(GstVideoDecoder *decoder,
                                  GstVideoCodecState *state)
{
    GstAmlV4l2Error error = GST_AML_V4L2_ERROR_INIT;
    gboolean ret = TRUE;
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);
    GstCaps *caps;

    GST_DEBUG_OBJECT(self, "Setting format: %" GST_PTR_FORMAT, state->caps);
    GstCapsFeatures *const features = gst_caps_get_features(state->caps, 0);

    if (gst_caps_features_contains(features, GST_CAPS_FEATURE_MEMORY_DMABUF))
        self->v4l2output->req_mode = GST_V4L2_IO_DMABUF_IMPORT;

    if (self->input_state)
    {
        if (gst_aml_v4l2_object_caps_equal(self->v4l2output, state->caps))
        {
            GST_DEBUG_OBJECT(self, "Compatible caps");
            goto done;
        }
        gst_video_codec_state_unref(self->input_state);
        self->input_state = NULL;

        gst_aml_v4l2_video_dec_finish(decoder);
        gst_aml_v4l2_object_stop(self->v4l2output);

        /* The renegotiation flow don't blend with the base class flow. To properly
         * stop the capture pool, if the buffers can't be orphaned, we need to
         * reclaim our buffers, which will happend through the allocation query.
         * The allocation query is triggered by gst_video_decoder_negotiate() which
         * requires the output caps to be set, but we can't know this information
         * as we rely on the decoder, which requires the capture queue to be
         * stopped.
         *
         * To workaround this issue, we simply run an allocation query with the
         * old negotiated caps in order to drain/reclaim our buffers. That breaks
         * the complexity and should not have much impact in performance since the
         * following allocation query will happen on a drained pipeline and won't
         * block. */
        if (self->v4l2capture->pool &&
            !gst_aml_v4l2_buffer_pool_orphan(&self->v4l2capture->pool))
        {
            GstCaps *caps = gst_pad_get_current_caps(decoder->srcpad);
            if (caps)
            {
                GstQuery *query = gst_query_new_allocation(caps, FALSE);
                gst_pad_peer_query(decoder->srcpad, query);
                gst_query_unref(query);
                gst_caps_unref(caps);
            }
        }

        gst_aml_v4l2_object_stop(self->v4l2capture);
        self->output_flow = GST_FLOW_OK;
    }

    if ((ret = gst_aml_v4l2_set_drm_mode(self->v4l2output)) == FALSE)
    {
        GST_ERROR_OBJECT(self, "config output drm mode error");
        goto done;
    }

    if ((ret = gst_aml_v4l2_set_stream_mode(self->v4l2output)) == FALSE)
    {
        GST_ERROR_OBJECT(self, "config output stream mode error");
        goto done;
    }

    ret = gst_aml_v4l2_object_set_format(self->v4l2output, state->caps, &error);

    gst_caps_replace(&self->probed_srccaps, NULL);
    self->probed_srccaps = gst_aml_v4l2_object_probe_caps(self->v4l2capture,
                                                          gst_aml_v4l2_object_get_raw_caps());

    if (gst_caps_is_empty(self->probed_srccaps))
        goto no_raw_format;

    caps = gst_caps_copy(self->probed_srccaps);
    gst_caps_set_features_simple(caps, gst_caps_features_from_string(GST_CAPS_FEATURE_MEMORY_DMABUF));
    gst_caps_append(self->probed_srccaps, caps);
    if (ret)
        self->input_state = gst_video_codec_state_ref(state);
    else
        gst_aml_v4l2_error(self, &error);

done:
    return ret;

no_raw_format:
    GST_ELEMENT_ERROR(self, RESOURCE, SETTINGS,
                      (_("Decoder on device %s has no supported output format"),
                       self->v4l2output->videodev),
                      (NULL));
    return GST_FLOW_ERROR;
}

static gboolean
gst_aml_v4l2_video_dec_flush(GstVideoDecoder *decoder)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);

    GST_DEBUG_OBJECT(self, "Flushed");

    /* Ensure the processing thread has stopped for the reverse playback
     * discount case */
    if (gst_pad_get_task_state(decoder->srcpad) == GST_TASK_STARTED)
    {
        GST_VIDEO_DECODER_STREAM_UNLOCK(decoder);

        gst_aml_v4l2_object_unlock(self->v4l2output);
        gst_aml_v4l2_object_unlock(self->v4l2capture);
        gst_pad_stop_task(decoder->srcpad);
        GST_VIDEO_DECODER_STREAM_LOCK(decoder);
    }

    self->output_flow = GST_FLOW_OK;

    gst_aml_v4l2_object_unlock_stop(self->v4l2output);
    gst_aml_v4l2_object_unlock_stop(self->v4l2capture);

    if (self->v4l2output->pool)
        gst_aml_v4l2_buffer_pool_flush(self->v4l2output->pool);

    /* gst_aml_v4l2_buffer_pool_flush() calls streamon the capture pool and must be
     * called after gst_aml_v4l2_object_unlock_stop() stopped flushing the buffer
     * pool. */
    if (self->v4l2capture->pool)
        gst_aml_v4l2_buffer_pool_flush(self->v4l2capture->pool);

    return TRUE;
}

static gboolean
gst_aml_v4l2_video_dec_negotiate(GstVideoDecoder *decoder)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);

    if (TRUE == self->v4l2output->is_svp)
    {
        GstStructure *s;
        GstEvent *event;

        s = gst_structure_new_empty ("IS_SVP");
        event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_STICKY, s);
        GST_DEBUG_OBJECT(self, "before Send SVP Event :%p", event);
        gst_pad_push_event (decoder->srcpad, event);
        GST_DEBUG_OBJECT(self, "after Send SVP Event :%p", event);
    }

    /* We don't allow renegotiation without carefull disabling the pool */
    if (self->v4l2capture->pool &&
        gst_buffer_pool_is_active(GST_BUFFER_POOL(self->v4l2capture->pool)))
        return TRUE;

    return GST_VIDEO_DECODER_CLASS(parent_class)->negotiate(decoder);
}

static gboolean
gst_aml_v4l2_decoder_cmd(GstAmlV4l2Object *v4l2object, guint cmd, guint flags)
{
    struct v4l2_decoder_cmd dcmd = {
        0,
    };

    GST_DEBUG_OBJECT(v4l2object->element,
                     "sending v4l2 decoder command %u with flags %u", cmd, flags);

    if (!GST_AML_V4L2_IS_OPEN(v4l2object))
        return FALSE;

    dcmd.cmd = cmd;
    dcmd.flags = flags;
    if (v4l2object->ioctl(v4l2object->video_fd, VIDIOC_DECODER_CMD, &dcmd) < 0)
        goto dcmd_failed;

    return TRUE;

dcmd_failed:
    if (errno == ENOTTY)
    {
        GST_INFO_OBJECT(v4l2object->element,
                        "Failed to send decoder command %u with flags %u for '%s'. (%s)",
                        cmd, flags, v4l2object->videodev, g_strerror(errno));
    }
    else
    {
        GST_ERROR_OBJECT(v4l2object->element,
                         "Failed to send decoder command %u with flags %u for '%s'. (%s)",
                         cmd, flags, v4l2object->videodev, g_strerror(errno));
    }
    return FALSE;
}

static GstFlowReturn
gst_aml_v4l2_video_dec_finish(GstVideoDecoder *decoder)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);
    GstFlowReturn ret = GST_FLOW_OK;
    GstBuffer *buffer;

    if (gst_pad_get_task_state(decoder->srcpad) != GST_TASK_STARTED)
        goto done;

    GST_DEBUG_OBJECT(self, "Finishing decoding");

    GST_VIDEO_DECODER_STREAM_UNLOCK(decoder);

    if (gst_aml_v4l2_decoder_cmd(self->v4l2output, V4L2_DEC_CMD_STOP, 0))
    {
        GstTask *task = decoder->srcpad->task;

        /* If the decoder stop command succeeded, just wait until processing is
         * finished */
        GST_DEBUG_OBJECT(self, "Waiting for decoder stop");
        GST_OBJECT_LOCK(task);
        while (GST_TASK_STATE(task) == GST_TASK_STARTED)
            GST_TASK_WAIT(task);
        GST_OBJECT_UNLOCK(task);
        ret = GST_FLOW_FLUSHING;
    }
    else
    {
        /* otherwise keep queuing empty buffers until the processing thread has
         * stopped, _pool_process() will return FLUSHING when that happened */
        while (ret == GST_FLOW_OK)
        {
            GST_DEBUG_OBJECT(self, "queue empty output buf");
            buffer = gst_buffer_new();
            ret =
                gst_aml_v4l2_buffer_pool_process(GST_AML_V4L2_BUFFER_POOL(self->v4l2output->pool), &buffer);
            gst_buffer_unref(buffer);
        }
    }

    /* and ensure the processing thread has stopped in case another error
     * occured. */
    gst_aml_v4l2_object_unlock(self->v4l2capture);
    gst_pad_stop_task(decoder->srcpad);
    GST_VIDEO_DECODER_STREAM_LOCK(decoder);

    if (ret == GST_FLOW_FLUSHING)
        ret = self->output_flow;

    GST_DEBUG_OBJECT(decoder, "Done draining buffers");

    /* TODO Shall we cleanup any reffed frame to workaround broken decoders ? */

done:
    return ret;
}

static GstFlowReturn
gst_aml_v4l2_video_dec_drain(GstVideoDecoder *decoder)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);

    GST_DEBUG_OBJECT(self, "Draining...");
    gst_aml_v4l2_video_dec_finish(decoder);
    gst_aml_v4l2_video_dec_flush(decoder);

    return GST_FLOW_OK;
}

static GstVideoCodecFrame *
gst_aml_v4l2_video_dec_get_oldest_frame(GstVideoDecoder *decoder)
{
    GstVideoCodecFrame *frame = NULL;
    GList *frames, *l;
    gint count = 0;

    frames = gst_video_decoder_get_frames(decoder);

    for (l = frames; l != NULL; l = l->next)
    {
        GstVideoCodecFrame *f = l->data;

        if (!frame || (GST_CLOCK_TIME_IS_VALID(frame->pts) && GST_CLOCK_TIME_IS_VALID(f->pts) && (frame->pts > f->pts)))
            frame = f;

        count++;
    }

    if (frame)
    {
        GST_LOG_OBJECT(decoder,
                       "Oldest frame is %d %" GST_TIME_FORMAT " and %d frames left",
                       frame->system_frame_number, GST_TIME_ARGS(frame->pts), count - 1);
        gst_video_codec_frame_ref(frame);
    }

    g_list_free_full(frames, (GDestroyNotify)gst_video_codec_frame_unref);

    return frame;
}

static GstVideoCodecFrame *
gst_aml_v4l2_video_dec_get_right_frame_for_frame_mode(GstVideoDecoder *decoder, GstClockTime pts)
{
    GstVideoCodecFrame *frame = NULL;
    GList *frames, *l;
    gint count = 0;

    GST_LOG_OBJECT (decoder, "trace in with pts: %" GST_TIME_FORMAT, GST_TIME_ARGS(pts));

    frames = gst_video_decoder_get_frames(decoder);

    for (l = frames; l != NULL; l = l->next)
    {
        GstVideoCodecFrame *f = l->data;

        if (GST_CLOCK_TIME_IS_VALID(pts) && (ABSDIFF(f->pts,pts)) < 1000) {
            frame = f;
        } else {
            if (!frame || (GST_CLOCK_TIME_IS_VALID(frame->pts) && GST_CLOCK_TIME_IS_VALID(f->pts) && (frame->pts > f->pts)))
                frame = f;
        }

        count++;
    }

    if (frame)
    {
        GST_LOG_OBJECT(decoder,
                       "frame is %d %" GST_TIME_FORMAT " and %d frames left",
                       frame->system_frame_number, GST_TIME_ARGS(frame->pts), count - 1);
        gst_video_codec_frame_ref(frame);
    }

    g_list_free_full(frames, (GDestroyNotify)gst_video_codec_frame_unref);

    GST_LOG_OBJECT (decoder, "trace out ret:%p", frame);
    return frame;
}

static GstVideoCodecFrame *
gst_aml_v4l2_video_dec_get_right_frame_for_stream_mode(GstVideoDecoder *decoder, GstClockTime pts)
{
    GstVideoCodecFrame *frame = NULL;
    GList *frames, *l;
    gint count = 0;

    GST_LOG_OBJECT (decoder, "trace in with pts: %" GST_TIME_FORMAT, GST_TIME_ARGS(pts));

    frames = gst_video_decoder_get_frames(decoder);
    guint frames_len = 0;
    frames_len = g_list_length(frames);
    GST_LOG_OBJECT (decoder, "got frames list len:%d", frames_len);

    frame = frames->data;

    for (l = frames; l != NULL; l = l->next)
    {
        GstVideoCodecFrame *f = l->data;

        if (GST_CLOCK_TIME_IS_VALID(pts) && (ABSDIFF(f->pts, pts)) < 1000)
        {
            /* found the right frame */
            frame = f;
            break;
        }
        else if(GST_CLOCK_TIME_IS_VALID(pts) && (f->pts < pts))
        {
            GST_LOG_OBJECT(decoder,
                "stream mode drop frame %d %" GST_TIME_FORMAT,
                frame->system_frame_number, GST_TIME_ARGS(frame->pts));

            gst_video_codec_frame_ref(f);
            // gst_video_decoder_drop_frame(decoder, f);
            gst_video_decoder_release_frame(decoder, f);
        }
        else
        {
            GST_LOG_OBJECT (decoder, "dbg");
        }
    }

    if (frame)
    {
        guint l_len = 0;
        l = gst_video_decoder_get_frames(decoder);
        l_len = g_list_length(l);
        g_list_free_full(l, (GDestroyNotify)gst_video_codec_frame_unref);

        GST_LOG_OBJECT(decoder,
                       "frame is %d %" GST_TIME_FORMAT " and %d frames left",
                       frame->system_frame_number, GST_TIME_ARGS(frame->pts), l_len);
        gst_video_codec_frame_ref(frame);
    }

    g_list_free_full(frames, (GDestroyNotify)gst_video_codec_frame_unref);

    GST_LOG_OBJECT (decoder, "trace out ret:%p", frame);
    return frame;
}

static GstVideoCodecFrame *
gst_aml_v4l2_video_dec_get_right_frame(GstVideoDecoder *decoder, GstClockTime pts)
{
    GstAmlV4l2VideoDec *self = (GstAmlV4l2VideoDec *)decoder;
    if (self->v4l2output->stream_mode)
        return gst_aml_v4l2_video_dec_get_right_frame_for_stream_mode(decoder, pts);
    else
        return gst_aml_v4l2_video_dec_get_right_frame_for_frame_mode(decoder, pts);
}

static gboolean
gst_aml_v4l2_video_remove_padding(GstCapsFeatures *features,
                                  GstStructure *structure, gpointer user_data)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(user_data);
    GstVideoAlignment *align = &self->v4l2capture->align;
    GstVideoInfo *info = &self->v4l2capture->info;
    int width, height;

    if (!gst_structure_get_int(structure, "width", &width))
        return TRUE;

    if (!gst_structure_get_int(structure, "height", &height))
        return TRUE;

    if (align->padding_left != 0 || align->padding_top != 0 ||
        height != info->height + align->padding_bottom)
        return TRUE;

    if (height == info->height + align->padding_bottom)
    {
        /* Some drivers may round up width to the padded with */
        if (width == info->width + align->padding_right)
            gst_structure_set(structure,
                              "width", G_TYPE_INT, width - align->padding_right,
                              "height", G_TYPE_INT, height - align->padding_bottom, NULL);
        /* Some drivers may keep visible width and only round up bytesperline */
        else if (width == info->width)
            gst_structure_set(structure,
                              "height", G_TYPE_INT, height - align->padding_bottom, NULL);
    }

    return TRUE;
}

static void
gst_v4l2_drop_event (GstAmlV4l2Object * v4l2object)
{
  struct v4l2_event evt;
  gint ret;

  memset (&evt, 0x00, sizeof (struct v4l2_event));
  ret = v4l2object->ioctl (v4l2object->video_fd, VIDIOC_DQEVENT, &evt);
  if (ret < 0)
  {
    GST_DEBUG_OBJECT (v4l2object, "dqevent failed");
    return;
  }

  switch (evt.type)
  {
    case V4L2_EVENT_SOURCE_CHANGE:
      GST_DEBUG_OBJECT (v4l2object, "Drop GST_V4L2_FLOW_SOURCE_CHANGE");
      break;
    case V4L2_EVENT_EOS:
      GST_DEBUG_OBJECT (v4l2object, "Drop GST_V4L2_FLOW_LAST_BUFFER");
      break;
    default:
      break;
  }

  return;
}

static void
gst_aml_v4l2_video_dec_loop(GstVideoDecoder *decoder)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);
    GstAmlV4l2BufferPool *v4l2_pool;
    GstAmlV4l2Error error = GST_AML_V4L2_ERROR_INIT;
    GstBufferPool *pool;
    GstVideoCodecFrame *frame;
    GstBuffer *buffer = NULL;
    GstFlowReturn ret;

    if (G_UNLIKELY(!GST_AML_V4L2_IS_ACTIVE(self->v4l2capture)))
    {
        GstVideoInfo info;
        GstVideoCodecState *output_state;
        GstCaps *acquired_caps, *available_caps, *caps, *filter;
        GstStructure *st;

        GST_DEBUG_OBJECT(self, "waitting source change event");
        /* Wait until received SOURCE_CHANGE event to get right video format */
        while (self->v4l2capture->can_wait_event && self->v4l2capture->need_wait_event)
        {
            ret = gst_aml_v4l2_object_dqevent(self->v4l2capture);
            if (ret == GST_AML_V4L2_FLOW_SOURCE_CHANGE)
            {
                GST_DEBUG_OBJECT(self, "Received source change event");
                break;
            }
            else if (ret == GST_AML_V4L2_FLOW_LAST_BUFFER)
            {
                GST_DEBUG_OBJECT(self, "Received eos event");
                goto beach;
            }
            else if (ret != GST_FLOW_OK)
            {
                GST_ERROR_OBJECT(self, "dqevent error");
                goto beach;
            }
        }
        self->v4l2capture->need_wait_event = FALSE;

        if (TRUE == self->v4l2output->is_svp)
        {
            GstPad *peer;
            GstStructure *s;
            GstEvent *event;

            peer = gst_pad_get_peer (decoder->srcpad);
            if (peer)
            {
              s = gst_structure_new_empty ("IS_SVP");
              event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, s);
              gst_pad_send_event (peer, event);
              GST_DEBUG_OBJECT(self, "Send SVP Event");
              gst_object_unref (peer);
            }
        }

        if (self->v4l2capture->need_drop_event)
        {
          // drop V4L2_EVENT_SOURCE_CHANGE
          gst_v4l2_drop_event(self->v4l2capture);
          self->v4l2capture->need_drop_event = FALSE;
        }

        if (!gst_aml_v4l2_object_acquire_format(self->v4l2capture, &info))
            goto not_negotiated;

        /* Create caps from the acquired format, remove the format field */
        acquired_caps = gst_video_info_to_caps(&info);
        GST_DEBUG_OBJECT(self, "Acquired caps: %" GST_PTR_FORMAT, acquired_caps);
        st = gst_caps_get_structure(acquired_caps, 0);
        gst_structure_remove_fields(st, "format", "colorimetry", "chroma-site", NULL);

        /* Probe currently available pixel formats */
        available_caps = gst_caps_copy(self->probed_srccaps);
        GST_DEBUG_OBJECT(self, "Available caps: %" GST_PTR_FORMAT, available_caps);

        /* Replace coded size with visible size, we want to negotiate visible size
         * with downstream, not coded size. */
        gst_caps_map_in_place(available_caps, gst_aml_v4l2_video_remove_padding, self);

        filter = gst_caps_intersect_full(available_caps, acquired_caps, GST_CAPS_INTERSECT_FIRST);
        caps = gst_caps_copy(filter);
        gst_caps_set_features_simple(caps, gst_caps_features_from_string(GST_CAPS_FEATURE_MEMORY_DMABUF));
        gst_caps_append(filter, caps);

        GST_DEBUG_OBJECT(self, "Filtered caps: %" GST_PTR_FORMAT, filter);
        gst_caps_unref(acquired_caps);
        gst_caps_unref(available_caps);
        caps = gst_pad_peer_query_caps(decoder->srcpad, filter);
        gst_caps_unref(filter);

        GST_DEBUG_OBJECT(self, "Possible decoded caps: %" GST_PTR_FORMAT, caps);
        if (gst_caps_is_empty(caps))
        {
            gst_caps_unref(caps);
            goto not_negotiated;
        }

        /* Fixate pixel format */
        caps = gst_caps_fixate(caps);

        GST_DEBUG_OBJECT(self, "Chosen decoded caps: %" GST_PTR_FORMAT, caps);

        /* Try to set negotiated format, on success replace acquired format */
        if (gst_aml_v4l2_object_set_format(self->v4l2capture, caps, &error))
            gst_video_info_from_caps(&info, caps);
        else
            gst_aml_v4l2_clear_error(&error);
        gst_caps_unref(caps);

        output_state = gst_video_decoder_set_output_state(decoder,
                                                          info.finfo->format, info.width, info.height, self->input_state);

        /* Copy the rest of the information, there might be more in the future */
        output_state->info.interlace_mode = info.interlace_mode;
        gst_video_codec_state_unref(output_state);

        if (!gst_video_decoder_negotiate(decoder))
        {
            if (GST_PAD_IS_FLUSHING(decoder->srcpad))
                goto flushing;
            else
                goto not_negotiated;
        }

        /* Ensure our internal pool is activated */
        if (!gst_buffer_pool_set_active(GST_BUFFER_POOL(self->v4l2capture->pool),
                                        TRUE))
            goto activate_failed;

        g_mutex_lock(&self->res_chg_lock);
        GST_LOG_OBJECT(decoder, "signal resolution changed");
        self->is_res_chg = FALSE;
        g_cond_signal(&self->res_chg_cond);
        g_mutex_unlock(&self->res_chg_lock);
    }

    GST_LOG_OBJECT(decoder, "Allocate output buffer");

    v4l2_pool = GST_AML_V4L2_BUFFER_POOL(self->v4l2capture->pool);

    self->output_flow = GST_FLOW_OK;
    do
    {
        /* We cannot use the base class allotate helper since it taking the internal
         * stream lock. we know that the acquire may need to poll until more frames
         * comes in and holding this lock would prevent that.
         */
        pool = gst_video_decoder_get_buffer_pool(decoder);

        /* Pool may be NULL if we started going to READY state */
        if (pool == NULL)
        {
            GST_WARNING_OBJECT(decoder, "gst_video_decoder_get_buffer_pool goto beach");
            ret = GST_FLOW_FLUSHING;
            goto beach;
        }

        ret = gst_buffer_pool_acquire_buffer(pool, &buffer, NULL);
        //calculate a new pts for interlace stream
        if (ret == GST_FLOW_OK &&
             self->v4l2capture->info.interlace_mode == GST_VIDEO_INTERLACE_MODE_INTERLEAVED)
        {
            //if buffer pts is valid, reduce 1/2 duration
            if (GST_BUFFER_DURATION_IS_VALID(buffer))
            {
                GST_BUFFER_DURATION(buffer) = GST_BUFFER_DURATION(buffer)/2;
            }
            GST_BUFFER_FLAG_UNSET(buffer, GST_VIDEO_BUFFER_FLAG_INTERLACED);
            //reset pts
            if (GST_BUFFER_TIMESTAMP (buffer) == 0LL || self->last_out_pts == GST_BUFFER_TIMESTAMP (buffer))
            {
                double rate = ((double)self->input_state->info.fps_n/(double)self->input_state->info.fps_d)*2;
                GST_BUFFER_TIMESTAMP(buffer) = self->last_out_pts + 1000000000LL/rate;
            }
        }

        g_object_unref(pool);

        if (ret == GST_FLOW_OK && GST_BUFFER_FLAG_IS_SET(buffer,GST_AML_V4L2_BUFFER_FLAG_LAST_EMPTY)) {
          GST_LOG_OBJECT(decoder, "Get GST_AML_V4L2_FLOW_LAST_BUFFER");
          self->v4l2capture->need_drop_event = TRUE;
          gst_aml_v4l2_buffer_pool_process(v4l2_pool, &buffer);
          goto beach;
        }

        if (ret == GST_AML_V4L2_FLOW_SOURCE_CHANGE)
        {
            GST_LOG_OBJECT(decoder, "Get GST_AML_V4L2_FLOW_SOURCE_CHANGE");

            g_mutex_lock (&self->res_chg_lock);
            self->is_res_chg = TRUE;
            g_mutex_unlock (&self->res_chg_lock);

            gst_aml_v4l2_object_stop(self->v4l2capture);
            return;
        }

        if (ret != GST_FLOW_OK) {
            GST_WARNING_OBJECT(decoder, "gst_buffer_pool_acquire_buffer goto beach ret:%d",ret);
            goto beach;
        }

        GST_LOG_OBJECT(decoder, "Process output buffer (switching flow outstanding num:%d)", self->v4l2capture->outstanding_buf_num);
        ret = gst_aml_v4l2_buffer_pool_process(v4l2_pool, &buffer);

        GST_DEBUG_OBJECT(decoder, "send pts:%lld - %" GST_TIME_FORMAT, GST_BUFFER_PTS(buffer), GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));
        g_signal_emit (self, g_signals[SIGNAL_DECODED_PTS], 0, GST_BUFFER_PTS(buffer));

        if (ret == GST_AML_V4L2_FLOW_SOURCE_CHANGE)
        {
            gst_aml_v4l2_object_stop(self->v4l2capture);
            return;
        }

    } while (ret == GST_AML_V4L2_FLOW_CORRUPTED_BUFFER);

    if (ret != GST_FLOW_OK)
        goto beach;

    frame = gst_aml_v4l2_video_dec_get_right_frame(decoder, GST_BUFFER_TIMESTAMP (buffer));
    if (frame)
    {
        if (!GST_CLOCK_TIME_IS_VALID(frame->pts))
        {
            if (!GST_CLOCK_TIME_IS_VALID(self->last_out_pts))
            {
                if (GST_CLOCK_TIME_IS_VALID(frame->dts))
                {
                    GST_BUFFER_TIMESTAMP(buffer) = frame->dts;
                }
                else
                {
                    GST_WARNING_OBJECT (decoder,"sorry,we have no baseline to calculate pts");
                    goto beach;
                }
            }
            else
            {
                double rate = ((double)self->input_state->info.fps_n/(double)self->input_state->info.fps_d);
                GST_BUFFER_TIMESTAMP(buffer) = self->last_out_pts + 1000000000LL/rate;
            }
        }
        self->last_out_pts = GST_BUFFER_TIMESTAMP(buffer);
        frame->output_buffer = buffer;
        frame->pts = GST_BUFFER_TIMESTAMP(buffer);
        frame->duration = GST_BUFFER_DURATION(buffer);
        buffer = NULL;
        ret = gst_video_decoder_finish_frame(decoder, frame);

        if (ret != GST_FLOW_OK)
            goto beach;
    }
    else
    {
        GST_WARNING_OBJECT(decoder, "Decoder is producing too many buffers");
        gst_buffer_unref(buffer);
    }

    return;
    /* ERRORS */
not_negotiated:
{
    GST_ERROR_OBJECT(self, "not negotiated");
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto beach;
}
activate_failed:
{
    GST_ERROR_OBJECT(self, "Buffer pool activation failed");
    GST_ELEMENT_ERROR(self, RESOURCE, SETTINGS,
                      (_("Failed to allocate required memory.")),
                      ("Buffer pool activation failed"));
    ret = GST_FLOW_ERROR;
    goto beach;
}
flushing:
{
    ret = GST_FLOW_FLUSHING;
    goto beach;
}
beach:
    GST_DEBUG_OBJECT(decoder, "Leaving output thread: %s",
                     gst_flow_get_name(ret));

    gst_buffer_replace(&buffer, NULL);
    self->output_flow = ret;
    gst_aml_v4l2_object_unlock(self->v4l2output);
    gst_pad_pause_task(decoder->srcpad);
}

static GstFlowReturn
gst_aml_v4l2_video_dec_handle_frame(GstVideoDecoder *decoder,
                                    GstVideoCodecFrame *frame)
{
    GstAmlV4l2Error error = GST_AML_V4L2_ERROR_INIT;
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);
    GstBufferPool *pool = GST_BUFFER_POOL(self->v4l2output->pool);
    GstFlowReturn ret = GST_FLOW_OK;
    gboolean processed = FALSE;
    GstBuffer *tmp;
    GstTaskState task_state;
    GstCaps *caps;

    GST_DEBUG_OBJECT(self, "Handling frame %d", frame->system_frame_number);

    if (G_UNLIKELY(!g_atomic_int_get(&self->active)))
        goto flushing;

    if (G_UNLIKELY(!GST_AML_V4L2_IS_ACTIVE(self->v4l2output)))
    {
        if (!self->input_state)
            goto not_negotiated;
        if (!gst_aml_v4l2_object_set_format(self->v4l2output, self->input_state->caps,
                                            &error))
            goto not_negotiated;
    }

    if (G_UNLIKELY(!GST_AML_V4L2_IS_ACTIVE(self->v4l2capture)))
    {
        GstBuffer *codec_data;
        GstCapsFeatures *features = NULL;

        features = gst_caps_get_features(self->input_state->caps, 0);
        if (features && gst_caps_features_contains(features, GST_CAPS_FEATURE_MEMORY_DMABUF))
        {
            GST_DEBUG_OBJECT(self, "Is SVP");
            self->v4l2output->is_svp = TRUE;
        }

        GST_DEBUG_OBJECT(self, "Sending header");

        codec_data = self->input_state->codec_data;

        /* We are running in byte-stream mode, so we don't know the headers, but
         * we need to send something, otherwise the decoder will refuse to
         * intialize.
         */
        if (codec_data)
        {
            gst_buffer_ref(codec_data);
        }
        else
        {
            codec_data = gst_buffer_ref(frame->input_buffer);
            processed = TRUE;
        }

        /* Ensure input internal pool is active */
        if (!gst_buffer_pool_is_active(pool))
        {
            GstStructure *config = gst_buffer_pool_get_config(pool);
            // guint min = MAX(self->v4l2output->min_buffers, GST_AML_V4L2_MIN_BUFFERS);
            // guint max = VIDEO_MAX_FRAME;
            //      gst_buffer_pool_config_set_params (config, self->input_state->caps,
            //          self->v4l2output->info.size, min, max);
            gst_buffer_pool_config_set_params(config, self->input_state->caps, self->v4l2output->info.size, self->v4l2output->min_buffers, self->v4l2output->min_buffers);

            /* There is no reason to refuse this config */
            if (!gst_buffer_pool_set_config(pool, config))
                goto activate_failed;
            GST_DEBUG_OBJECT(self, "setting output pool config to %" GST_PTR_FORMAT, config);

            if (!gst_buffer_pool_set_active(pool, TRUE))
                goto activate_failed;
        }

        GST_VIDEO_DECODER_STREAM_UNLOCK(decoder);
        ret =
            gst_aml_v4l2_buffer_pool_process(GST_AML_V4L2_BUFFER_POOL(self->v4l2output->pool), &codec_data);
        GST_VIDEO_DECODER_STREAM_LOCK(decoder);

        gst_buffer_unref(codec_data);

        /* For decoders G_FMT returns coded size, G_SELECTION returns visible size
         * in the compose rectangle. gst_aml_v4l2_object_acquire_format() checks both
         * and returns the visible size as with/height and the coded size as
         * padding. */
    }

    task_state = gst_pad_get_task_state(GST_VIDEO_DECODER_SRC_PAD(self));
    if (task_state == GST_TASK_STOPPED || task_state == GST_TASK_PAUSED)
    {
        /* It's possible that the processing thread stopped due to an error */
        if (self->output_flow != GST_FLOW_OK &&
            self->output_flow != GST_FLOW_FLUSHING)
        {
            GST_DEBUG_OBJECT(self, "Processing loop stopped with error, leaving");
            ret = self->output_flow;
            goto drop;
        }

        GST_DEBUG_OBJECT(self, "Starting decoding thread");

        /* Start the processing task, when it quits, the task will disable input
         * processing to unlock input if draining, or prevent potential block */
        self->output_flow = GST_FLOW_FLUSHING;
        if (!gst_pad_start_task(decoder->srcpad,
                                (GstTaskFunction)gst_aml_v4l2_video_dec_loop, self, NULL))
            goto start_task_failed;
    }

    if (!processed)
    {
        GST_VIDEO_DECODER_STREAM_UNLOCK(decoder);
        ret =
            gst_aml_v4l2_buffer_pool_process(GST_AML_V4L2_BUFFER_POOL(self->v4l2output->pool), &frame->input_buffer);
        GST_VIDEO_DECODER_STREAM_LOCK(decoder);

        if (ret == GST_FLOW_FLUSHING)
        {
            if (gst_pad_get_task_state(GST_VIDEO_DECODER_SRC_PAD(self)) !=
                GST_TASK_STARTED)
                ret = self->output_flow;
            goto drop;
        }
        else if (ret != GST_FLOW_OK)
        {
            goto process_failed;
        }
    }

    /* No need to keep input arround */
    tmp = frame->input_buffer;
    frame->input_buffer = gst_buffer_new();
    gst_buffer_copy_into(frame->input_buffer, tmp,
                         GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS |
                             GST_BUFFER_COPY_META,
                         0, 0);
    gst_buffer_unref(tmp);

    gst_video_codec_frame_unref(frame);
    return ret;

    /* ERRORS */
not_negotiated:
{
    GST_ERROR_OBJECT(self, "not negotiated");
    ret = GST_FLOW_NOT_NEGOTIATED;
    gst_aml_v4l2_error(self, &error);
    goto drop;
}
activate_failed:
{
    GST_ELEMENT_ERROR(self, RESOURCE, SETTINGS,
                      (_("Failed to allocate required memory.")),
                      ("Buffer pool activation failed"));
    ret = GST_FLOW_ERROR;
    goto drop;
}
flushing:
{
    ret = GST_FLOW_FLUSHING;
    goto drop;
}

start_task_failed:
{
    GST_ELEMENT_ERROR(self, RESOURCE, FAILED,
                      (_("Failed to start decoding thread.")), (NULL));
    ret = GST_FLOW_ERROR;
    goto drop;
}
process_failed:
{
    GST_ELEMENT_ERROR(self, RESOURCE, FAILED,
                      (_("Failed to process frame.")),
                      ("Maybe be due to not enough memory or failing driver"));
    ret = GST_FLOW_ERROR;
    goto drop;
}
drop:
{
    gst_video_decoder_drop_frame(decoder, frame);
    return ret;
}
}

static gboolean
gst_aml_v4l2_video_dec_decide_allocation(GstVideoDecoder *decoder,
                                         GstQuery *query)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);
    GstClockTime latency;
    gboolean ret = FALSE;

    if (gst_aml_v4l2_object_decide_allocation(self->v4l2capture, query))
        ret = GST_VIDEO_DECODER_CLASS(parent_class)->decide_allocation(decoder, query);

    if (GST_CLOCK_TIME_IS_VALID(self->v4l2capture->duration))
    {
        latency = self->v4l2capture->min_buffers * self->v4l2capture->duration;
        GST_DEBUG_OBJECT(self, "Setting latency: %" GST_TIME_FORMAT " (%" G_GUINT32_FORMAT " * %" G_GUINT64_FORMAT, GST_TIME_ARGS(latency),
                         self->v4l2capture->min_buffers, self->v4l2capture->duration);
        gst_video_decoder_set_latency(decoder, latency, latency);
    }
    else
    {
        GST_WARNING_OBJECT(self, "Duration invalid, not setting latency");
    }

    return ret;
}

static gboolean
gst_aml_v4l2_video_dec_src_query(GstVideoDecoder *decoder, GstQuery *query)
{
    gboolean ret = TRUE;
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);

    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_CAPS:
    {
        GstCaps *filter, *result = NULL;
        GstPad *pad = GST_VIDEO_DECODER_SRC_PAD(decoder);

        gst_query_parse_caps(query, &filter);

        if (self->probed_srccaps)
            result = gst_caps_ref(self->probed_srccaps);
        else
            result = gst_pad_get_pad_template_caps(pad);

        if (filter)
        {
            GstCaps *tmp = result;
            result =
                gst_caps_intersect_full(filter, tmp, GST_CAPS_INTERSECT_FIRST);
            gst_caps_unref(tmp);
        }

        GST_DEBUG_OBJECT(self, "Returning src caps %" GST_PTR_FORMAT, result);

        gst_query_set_caps_result(query, result);
        gst_caps_unref(result);
        break;
    }

    default:
        ret = GST_VIDEO_DECODER_CLASS(parent_class)->src_query(decoder, query);
        break;
    }

    return ret;
}

static GstCaps *
gst_aml_v4l2_video_dec_sink_getcaps(GstVideoDecoder *decoder, GstCaps *filter)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);
    GstCaps *result;

    result = gst_video_decoder_proxy_getcaps(decoder, self->probed_sinkcaps,
                                             filter);

    GST_DEBUG_OBJECT(self, "Returning sink caps %" GST_PTR_FORMAT, result);

    return result;
}

static gboolean
gst_aml_v4l2_video_dec_sink_event(GstVideoDecoder *decoder, GstEvent *event)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(decoder);
    gboolean ret;
    GstEventType type = GST_EVENT_TYPE(event);

    switch (type)
    {
    case GST_EVENT_STREAM_START:
    {
        GstStructure *s;
        GstEvent *event;
        GST_DEBUG_OBJECT(self, "new private event");
        s = gst_structure_new("private_signal", "obj_ptr", G_TYPE_POINTER, self, "sig_name", G_TYPE_STRING, "decoded-pts", NULL);
        event = gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s);
        GST_DEBUG_OBJECT(self, "before Send private_signal Event :%p", event);
        gst_pad_push_event (decoder->sinkpad, event);
        GST_DEBUG_OBJECT(self, "after Send private_signal Event :%p", event);
        break;
    }
    case GST_EVENT_FLUSH_START:
        GST_DEBUG_OBJECT(self, "flush start");

        g_mutex_lock (&self->res_chg_lock);
        while (self->is_res_chg)
        {
            GST_LOG_OBJECT(decoder, "wait resolution change finish");
            g_cond_wait(&self->res_chg_cond, &self->res_chg_lock);
        }
        g_mutex_unlock (&self->res_chg_lock);

        self->last_out_pts = GST_CLOCK_TIME_NONE;
        gst_aml_v4l2_object_unlock(self->v4l2output);
        gst_aml_v4l2_object_unlock(self->v4l2capture);
        break;
    default:
        break;
    }

    ret = GST_VIDEO_DECODER_CLASS(parent_class)->sink_event(decoder, event);

    switch (type)
    {
    case GST_EVENT_FLUSH_START:
        /* The processing thread should stop now, wait for it */
        gst_pad_stop_task(decoder->srcpad);
        GST_DEBUG_OBJECT(self, "flush start done");
        break;
    default:
        break;
    }

    return ret;
}

static GstStateChangeReturn
gst_aml_v4l2_video_dec_change_state(GstElement *element,
                                    GstStateChange transition)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(element);
    GstVideoDecoder *decoder = GST_VIDEO_DECODER(element);

    if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
    {
        g_atomic_int_set(&self->active, FALSE);
        gst_aml_v4l2_object_unlock(self->v4l2output);
        gst_aml_v4l2_object_unlock(self->v4l2capture);
        gst_pad_stop_task(decoder->srcpad);
    }

    return GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
}

static void
gst_aml_v4l2_video_dec_dispose(GObject *object)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(object);

    gst_caps_replace(&self->probed_sinkcaps, NULL);
    gst_caps_replace(&self->probed_srccaps, NULL);

    G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
gst_aml_v4l2_video_dec_finalize(GObject *object)
{
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(object);

    gst_aml_v4l2_object_destroy(self->v4l2capture);
    gst_aml_v4l2_object_destroy(self->v4l2output);

    g_mutex_clear(&self->res_chg_lock);
    g_cond_clear(&self->res_chg_cond);

#if GST_IMPORT_LGE_PROP
    if (self->lge_ctxt)
    {
        if (self->lge_ctxt->app_type)
            g_free(self->lge_ctxt->app_type);
        if (self->lge_ctxt->res_info.coretype)
            g_free(self->lge_ctxt->res_info.coretype);
        free(self->lge_ctxt);
    }

#endif

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gst_aml_v4l2_video_dec_init(GstAmlV4l2VideoDec *self)
{
    /* V4L2 object are created in subinstance_init */
    self->last_out_pts = GST_CLOCK_TIME_NONE;
    self->is_secure_path = FALSE;
    self->is_res_chg = FALSE;
    g_mutex_init(&self->res_chg_lock);
    g_cond_init(&self->res_chg_cond);
#if GST_IMPORT_LGE_PROP
    self->lge_ctxt = malloc(sizeof(GstAmlV4l2VideoDecLgeCtxt));
    memset(self->lge_ctxt, 0, sizeof(GstAmlV4l2VideoDecLgeCtxt));
#endif
}

static void
gst_aml_v4l2_video_dec_subinstance_init(GTypeInstance *instance, gpointer g_class)
{
    GstAmlV4l2VideoDecClass *klass = GST_AML_V4L2_VIDEO_DEC_CLASS(g_class);
    GstAmlV4l2VideoDec *self = GST_AML_V4L2_VIDEO_DEC(instance);
    GstVideoDecoder *decoder = GST_VIDEO_DECODER(instance);

    gst_video_decoder_set_packetized(decoder, TRUE);

    self->v4l2output = gst_aml_v4l2_object_new(GST_ELEMENT(self),
                                               GST_OBJECT(GST_VIDEO_DECODER_SINK_PAD(self)),
                                               V4L2_BUF_TYPE_VIDEO_OUTPUT, klass->default_device,
                                               gst_aml_v4l2_get_output, gst_aml_v4l2_set_output, NULL);
    self->v4l2output->no_initial_format = TRUE;
    self->v4l2output->keep_aspect = FALSE;
    self->v4l2output->is_svp = FALSE;

    self->v4l2capture = gst_aml_v4l2_object_new(GST_ELEMENT(self),
                                                GST_OBJECT(GST_VIDEO_DECODER_SRC_PAD(self)),
                                                V4L2_BUF_TYPE_VIDEO_CAPTURE, klass->default_device,
                                                gst_aml_v4l2_get_input, gst_aml_v4l2_set_input, NULL);
    self->v4l2capture->need_wait_event = TRUE;
    self->v4l2capture->need_drop_event = FALSE;
}

static void
gst_aml_v4l2_video_dec_class_init(GstAmlV4l2VideoDecClass *klass)
{
    GstElementClass *element_class;
    GObjectClass *gobject_class;
    GstVideoDecoderClass *video_decoder_class;

    parent_class = g_type_class_peek_parent(klass);

    element_class = (GstElementClass *)klass;
    gobject_class = (GObjectClass *)klass;
    video_decoder_class = (GstVideoDecoderClass *)klass;

    GST_DEBUG_CATEGORY_INIT(gst_aml_v4l2_video_dec_debug, "amlv4l2videodec", 0,
                            "AML V4L2 Video Decoder");

    gobject_class->dispose = GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_dispose);
    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_finalize);
    gobject_class->set_property =
        GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_set_property);
    gobject_class->get_property =
        GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_get_property);

    video_decoder_class->open = GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_open);
    video_decoder_class->close = GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_close);
    video_decoder_class->start = GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_start);
    video_decoder_class->stop = GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_stop);
    video_decoder_class->finish = GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_finish);
    video_decoder_class->flush = GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_flush);
    video_decoder_class->drain = GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_drain);
    video_decoder_class->set_format =
        GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_set_format);
    video_decoder_class->negotiate =
        GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_negotiate);
    video_decoder_class->decide_allocation =
        GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_decide_allocation);
    /* FIXME propose_allocation or not ? */
    video_decoder_class->handle_frame =
        GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_handle_frame);
    video_decoder_class->getcaps =
        GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_sink_getcaps);
    video_decoder_class->src_query =
        GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_src_query);
    video_decoder_class->sink_event =
        GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_sink_event);

    element_class->change_state =
        GST_DEBUG_FUNCPTR(gst_aml_v4l2_video_dec_change_state);

  g_signals[SIGNAL_DECODED_PTS] = g_signal_new ("decoded-pts",
        G_TYPE_FROM_CLASS(GST_ELEMENT_CLASS(klass)),
        G_SIGNAL_RUN_LAST,
        0,    /* class offset */
        NULL, /* accumulator */
        NULL, /* accu data */
        g_cclosure_marshal_generic,
        G_TYPE_NONE,
        1,
        G_TYPE_UINT64);

    gst_aml_v4l2_object_install_m2m_properties_helper(gobject_class);
#if GST_IMPORT_LGE_PROP
    gst_aml_v4l2_video_dec_install_lge_properties_helper(gobject_class);
#endif
}

static void
gst_aml_v4l2_video_dec_subclass_init(gpointer g_class, gpointer data)
{
    GstAmlV4l2VideoDecClass *klass = GST_AML_V4L2_VIDEO_DEC_CLASS(g_class);
    GstElementClass *element_class = GST_ELEMENT_CLASS(g_class);
    GstAmlV4l2VideoDecCData *cdata = data;

    klass->default_device = cdata->device;

    /* Note: gst_pad_template_new() take the floating ref from the caps */
    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                            cdata->sink_caps));
    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                            cdata->src_caps));

    gst_element_class_set_metadata(element_class, cdata->longname,
                                   "Codec/Decoder/Video/Hardware", cdata->description,
                                   "Xuesong Jiang <Xuesong.Jiang@amlogic.com>");

    gst_caps_unref(cdata->sink_caps);
    gst_caps_unref(cdata->src_caps);
    g_free(cdata);
}

/* Probing functions */
gboolean
gst_aml_v4l2_is_video_dec(GstCaps *sink_caps, GstCaps *src_caps)
{
    gboolean ret = FALSE;

    if (gst_caps_is_subset(sink_caps, gst_aml_v4l2_object_get_codec_caps()) && gst_caps_is_subset(src_caps, gst_aml_v4l2_object_get_raw_caps()))
        ret = TRUE;

    return ret;
}

static gchar *
gst_aml_v4l2_video_dec_set_metadata(GstStructure *s, GstAmlV4l2VideoDecCData *cdata,
                                    const gchar *basename)
{
    gchar *codec_name = NULL;
    gchar *type_name = NULL;
    gboolean got_value = FALSE;

#define SET_META(codec)                                                \
    G_STMT_START                                                       \
    {                                                                  \
        cdata->longname = "AML V4L2 " codec " Decoder";                \
        cdata->description = "Decodes " codec " streams via V4L2 API"; \
        codec_name = g_ascii_strdown(codec, -1);                       \
    }                                                                  \
    G_STMT_END

    if (gst_structure_has_name(s, "image/jpeg"))
    {
        SET_META("JPEG");
    }
    else if (gst_structure_has_name(s, "video/mpeg"))
    {
        gint mpegversion = 0;
        gint *list = NULL;
        got_value = gst_structure_get_int(s, "mpegversion", &mpegversion);
        if (FALSE == got_value)
        {
            got_value = gst_structure_get_list(s, "mpegversion", &list);
            if (TRUE == got_value && (1 == *list || 2 == *list))
            {
                SET_META("MPEG2");
            }
            else
            {
                SET_META("MPEG4");
            }
        }
        else
        {
            SET_META("MPEG4");
        }
    }
    else if (gst_structure_has_name(s, "video/x-h263"))
    {
        SET_META("H263");
    }
    else if (gst_structure_has_name(s, "video/x-fwht"))
    {
        SET_META("FWHT");
    }
    else if (gst_structure_has_name(s, "video/x-h264"))
    {
        SET_META("H264");
    }
    else if (gst_structure_has_name(s, "video/x-h265"))
    {
        SET_META("H265");
    }
    else if (gst_structure_has_name(s, "video/x-wmv"))
    {
        SET_META("VC1");
    }
    else if (gst_structure_has_name(s, "video/x-vp8"))
    {
        SET_META("VP8");
    }
    else if (gst_structure_has_name(s, "video/x-vp9"))
    {
        SET_META("VP9");
    }
    else if (gst_structure_has_name(s, "video/x-av1"))
    {
        SET_META("AV1");
    }
    else if (gst_structure_has_name(s, "video/x-bayer"))
    {
        SET_META("BAYER");
    }
    else if (gst_structure_has_name(s, "video/x-sonix"))
    {
        SET_META("SONIX");
    }
    else if (gst_structure_has_name(s, "video/x-pwc1"))
    {
        SET_META("PWC1");
    }
    else if (gst_structure_has_name(s, "video/x-pwc2"))
    {
        SET_META("PWC2");
    }
    else
    {
        /* This code should be kept on sync with the exposed CODEC type of format
         * from gstamlv4l2object.c. This warning will only occure in case we forget
         * to also add a format here. */
        gchar *s_str = gst_structure_to_string(s);
        g_warning("Missing fixed name mapping for caps '%s', this is a GStreamer "
                  "bug, please report at https://bugs.gnome.org",
                  s_str);
        g_free(s_str);
    }

    if (codec_name)
    {
        type_name = g_strdup_printf("amlv4l2%sdec", codec_name);
        if (g_type_from_name(type_name) != 0)
        {
            g_free(type_name);
            type_name = g_strdup_printf("amlv4l2%s%sdec", basename, codec_name);
        }

        g_free(codec_name);
    }

    return type_name;
#undef SET_META
}

void gst_aml_v4l2_video_dec_register(GstPlugin *plugin, const gchar *basename,
                                     const gchar *device_path, GstCaps *sink_caps, GstCaps *src_caps)
{
    gint i;

    for (i = 0; i < gst_caps_get_size(sink_caps); i++)
    {
        GstAmlV4l2VideoDecCData *cdata;
        GstStructure *s;
        GTypeQuery type_query;
        GTypeInfo type_info = {
            0,
        };
        GType type, subtype;
        gchar *type_name;

        s = gst_caps_get_structure(sink_caps, i);

        cdata = g_new0(GstAmlV4l2VideoDecCData, 1);
        cdata->device = g_strdup(device_path);
        cdata->sink_caps = gst_caps_new_empty();
        gst_caps_append_structure(cdata->sink_caps, gst_structure_copy(s));
        gst_caps_append_structure(cdata->sink_caps, gst_structure_copy(s));
        gst_caps_set_features(cdata->sink_caps, 0, gst_caps_features_from_string(GST_CAPS_FEATURE_MEMORY_DMABUF));
        cdata->src_caps = gst_caps_copy(src_caps);
        gst_caps_set_features_simple(cdata->src_caps, gst_caps_features_from_string(GST_CAPS_FEATURE_MEMORY_DMABUF));
        gst_caps_append(cdata->src_caps, gst_caps_copy(src_caps));
        type_name = gst_aml_v4l2_video_dec_set_metadata(s, cdata, basename);

        /* Skip over if we hit an unmapped type */
        if (!type_name)
        {
            g_free(cdata);
            continue;
        }

        type = gst_aml_v4l2_video_dec_get_type();
        g_type_query(type, &type_query);
        memset(&type_info, 0, sizeof(type_info));
        type_info.class_size = type_query.class_size;
        type_info.instance_size = type_query.instance_size;
        type_info.class_init = gst_aml_v4l2_video_dec_subclass_init;
        type_info.class_data = cdata;
        type_info.instance_init = gst_aml_v4l2_video_dec_subinstance_init;

        subtype = g_type_register_static(type, type_name, &type_info, 0);
        if (!gst_element_register(plugin, type_name, GST_RANK_PRIMARY + 1,
                                  subtype))
            GST_WARNING("Failed to register plugin '%s'", type_name);

        g_free(type_name);
    }
}

#if GST_IMPORT_LGE_PROP
static void gst_aml_v4l2_video_dec_install_lge_properties_helper(GObjectClass *gobject_class)
{
    g_object_class_install_property(gobject_class, LGE_RESOURCE_INFO,
                                    g_param_spec_object("resource-info", "resource-info",
                                                        "After acquisition of H/W resources is completed, allocated resource information must be delivered to the decoder and the sink",
                                                        GST_TYPE_STRUCTURE,
                                                        G_PARAM_READABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, LGE_DECODE_SIZE,
                                    g_param_spec_uint64("decoded-size", "decoded-size",
                                                        "The total amount of decoder element's decoded video es after constructing pipeline or flushing pipeline update unit is byte.",
                                                        0, G_MAXUINT64,
                                                        0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, LGE_UNDECODE_SIZE,
                                    g_param_spec_uint64("undecoded-size", "undecoded-size",
                                                        "video decoder element's total undecoded data update unit is byte.",
                                                        0, G_MAXUINT64,
                                                        0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, LGE_APP_TYPE,
                                    g_param_spec_string("app-type", "app-type",
                                                        "set application type.",
                                                        "default_app",
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, LGE_CLIP_MODE,
                                    g_param_spec_boolean("clip-mode", "clip-mode",
                                                         "When seeking, Content is moving faster for a while to skip frames.",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}
#endif
