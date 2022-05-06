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

#include "gst/gst-i18n-plugin.h"

#include <gst/gst.h>

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ext/videodev2.h"
#include "aml-v4l2-utils.h"

#include "gstamlv4l2object.h"
#include "gstamlv4l2videodec.h"

/* used in gstamlv4l2object.c and aml_v4l2_calls.c */
GST_DEBUG_CATEGORY(aml_v4l2_debug);
#define GST_CAT_DEFAULT aml_v4l2_debug

/* This is a minimalist probe, for speed, we only enumerate formats */
static GstCaps *
gst_aml_v4l2_probe_template_caps(const gchar *device, gint video_fd,
                                 enum v4l2_buf_type type)
{
    gint n;
    struct v4l2_fmtdesc format;
    GstCaps *caps;

    GST_DEBUG("Getting %s format enumerations", device);
    caps = gst_caps_new_empty();

    for (n = 0;; n++)
    {
        GstStructure *template;

        memset(&format, 0, sizeof(format));

        format.index = n;
        format.type = type;

        if (ioctl(video_fd, VIDIOC_ENUM_FMT, &format) < 0)
            break; /* end of enumeration */

        GST_LOG("index:       %u", format.index);
        GST_LOG("type:        %d", format.type);
        GST_LOG("flags:       %08x", format.flags);
        GST_LOG("description: '%s'", format.description);
        GST_LOG("pixelformat: %" GST_FOURCC_FORMAT,
                GST_FOURCC_ARGS(format.pixelformat));

        template = gst_aml_v4l2_object_v4l2fourcc_to_structure(format.pixelformat);

        if (template)
        {
            GstStructure *alt_t = NULL;

            switch (format.pixelformat)
            {
            case V4L2_PIX_FMT_RGB32:
                alt_t = gst_structure_copy(template);
                gst_structure_set(alt_t, "format", G_TYPE_STRING, "ARGB", NULL);
                break;
            case V4L2_PIX_FMT_BGR32:
                alt_t = gst_structure_copy(template);
                gst_structure_set(alt_t, "format", G_TYPE_STRING, "BGRA", NULL);
            default:
                break;
            }

            gst_caps_append_structure(caps, template);

            if (alt_t)
                gst_caps_append_structure(caps, alt_t);
        }
    }

    return gst_caps_simplify(caps);
}

static gboolean
gst_aml_v4l2_probe_and_register(GstPlugin *plugin)
{
    GstAmlV4l2Iterator *it;
    gint video_fd = -1;
    struct v4l2_capability vcap;
    guint32 device_caps;

    GST_DEBUG("Probing devices");

    it = gst_aml_v4l2_iterator_new();

    while (gst_aml_v4l2_iterator_next(it))
    {
        GstCaps *src_caps, *sink_caps;
        gchar *basename;

        if (video_fd >= 0)
            close(video_fd);

        video_fd = open(it->device_path, O_RDWR | O_CLOEXEC);

        if (video_fd == -1)
        {
            GST_DEBUG("Failed to open %s: %s", it->device_path, g_strerror(errno));
            continue;
        }

        memset(&vcap, 0, sizeof(vcap));

        if (ioctl(video_fd, VIDIOC_QUERYCAP, &vcap) < 0)
        {
            GST_DEBUG("Failed to get device capabilities: %s", g_strerror(errno));
            continue;
        }

        if (vcap.capabilities & V4L2_CAP_DEVICE_CAPS)
            device_caps = vcap.device_caps;
        else
            device_caps = vcap.capabilities;

        if (!GST_AML_V4L2_IS_M2M(device_caps))
            continue;

        GST_DEBUG("Probing '%s' located at '%s'",
                  it->device_name ? it->device_name : (const gchar *)vcap.driver,
                  it->device_path);

        /* get sink supported format (no MPLANE for codec) */
        sink_caps = gst_caps_merge(gst_aml_v4l2_probe_template_caps(it->device_path,
                                                                    video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT),
                                   gst_aml_v4l2_probe_template_caps(it->device_path, video_fd,
                                                                    V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));

        /* get src supported format */
        src_caps = gst_caps_merge(gst_aml_v4l2_probe_template_caps(it->device_path,
                                                                   video_fd, V4L2_BUF_TYPE_VIDEO_CAPTURE),
                                  gst_aml_v4l2_probe_template_caps(it->device_path, video_fd,
                                                                   V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE));

        /* Skip devices without any supported formats */
        if (gst_caps_is_empty(sink_caps) || gst_caps_is_empty(src_caps))
        {
            gst_caps_unref(sink_caps);
            gst_caps_unref(src_caps);
            continue;
        }

        basename = g_path_get_basename(it->device_path);

        /* Caps won't be freed if the subclass is not instantiated */
        GST_MINI_OBJECT_FLAG_SET(sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
        GST_MINI_OBJECT_FLAG_SET(src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

        if (gst_aml_v4l2_is_video_dec(sink_caps, src_caps))
        {
            gst_aml_v4l2_video_dec_register(plugin, basename, it->device_path,
                                            sink_caps, src_caps);
        }

        gst_caps_unref(sink_caps);
        gst_caps_unref(src_caps);
        g_free(basename);
    }

    if (video_fd >= 0)
        close(video_fd);

    gst_aml_v4l2_iterator_free(it);

    return TRUE;
}

static gboolean
plugin_init(GstPlugin *plugin)
{
    const gchar *paths[] = {"/dev", "/dev/v4l2", NULL};
    const gchar *names[] = {"video", NULL};

    GST_DEBUG_CATEGORY_INIT(aml_v4l2_debug, "amlv4l2", 0, "aml V4L2 API calls");

    /* Add some depedency, so the dynamic features get updated upon changes in
     * /dev/video* */
    gst_plugin_add_dependency(plugin,
                              NULL, paths, names, GST_PLUGIN_DEPENDENCY_FLAG_FILE_NAME_IS_PREFIX);

    if (!gst_aml_v4l2_probe_and_register(plugin))
        return FALSE;

    return TRUE;
}

#ifndef VERSION
#define VERSION "0.1.0"
#endif
#ifndef PACKAGE
#define PACKAGE "aml_package"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "aml_media"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://amlogic.com"
#endif

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  amlv4l2,
                  "aml element for Video 4 Linux decoder",
                  plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
