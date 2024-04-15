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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __AML_VDEC_H__
#define __AML_VDEC_H__

typedef unsigned int uint32_t;

enum vdec_dw_mode
{
    VDEC_DW_AFBC_ONLY = 0,
    VDEC_DW_AFBC_1_1_DW = 1,
    VDEC_DW_AFBC_1_4_DW = 2,
    VDEC_DW_AFBC_x2_1_4_DW = 3,
    VDEC_DW_AFBC_1_2_DW = 4,
    VDEC_DW_NO_AFBC = 16,
    /* MMU Double Write Mode for filmgrain*/
    VDEC_DW_MMU_1 = 0x21,
    VDEC_DW_MMU_1_4 = 0x22,
    VDEC_DW_MMU_1_2 = 0x24,
    VDEC_DW_AFBC_AUTO_1_2 = 0x100,
    VDEC_DW_AFBC_AUTO_1_4 = 0x200,
};

struct aml_vdec_cfg_infos
{
    uint32_t double_write_mode;
    uint32_t init_width;
    uint32_t init_height;
    uint32_t ref_buf_margin;
    uint32_t canvas_mem_mode;
    uint32_t canvas_mem_endian;
    uint32_t low_latency_mode;
    uint32_t uvm_hook_type;
    /*
     * bit 16       : force progressive output flag.
     * bit 15       : enable nr.
     * bit 14       : enable di local buff.
     * bit 13       : report downscale yuv buffer size flag.
     * bit 12       : for second field pts mode.default value 1.
     * bit 1        : Non-standard dv flag.
     * bit 0        : dv two layer flag.
     */
    uint32_t metadata_config_flag; // for metadata config flag
    uint32_t data[5];
};

/* content_light_level from SEI */
struct vframe_content_light_level_s
{
    uint32_t present_flag;
    uint32_t max_content;
    uint32_t max_pic_average;
};

/* master_display_colour_info_volume from SEI */
struct vframe_master_display_colour_s
{
    uint32_t present_flag;
    uint32_t primaries[3][2];
    uint32_t white_point[2];
    uint32_t luminance[2];
    struct vframe_content_light_level_s content_light_level;
};

struct aml_vdec_hdr_infos
{
    /*
     * bit 29   : present_flag
     * bit 28-26: video_format "component", "PAL", "NTSC", "SECAM", "MAC", "unspecified"
     * bit 25   : range "limited", "full_range"
     * bit 24   : color_description_present_flag
     * bit 23-16: color_primaries "unknown", "bt709", "undef", "bt601",
     *            "bt470m", "bt470bg", "smpte170m", "smpte240m", "film", "bt2020"
     * bit 15-8 : transfer_characteristic unknown", "bt709", "undef", "bt601",
     *            "bt470m", "bt470bg", "smpte170m", "smpte240m",
     *            "linear", "log100", "log316", "iec61966-2-4",
     *            "bt1361e", "iec61966-2-1", "bt2020-10", "bt2020-12",
     *            "smpte-st-2084", "smpte-st-428"
     * bit 7-0  : matrix_coefficient "GBR", "bt709", "undef", "bt601",
     *            "fcc", "bt470bg", "smpte170m", "smpte240m",
     *            "YCgCo", "bt2020nc", "bt2020c"
     */
    uint32_t signal_type;
    struct vframe_master_display_colour_s color_parms;
};

struct aml_vdec_ps_infos
{
    uint32_t visible_width;
    uint32_t visible_height;
    uint32_t coded_width;
    uint32_t coded_height;
    uint32_t profile;
    uint32_t mb_width;
    uint32_t mb_height;
    uint32_t dpb_size;
    uint32_t ref_frames;
    uint32_t reorder_frames;
    uint32_t reorder_margin;
    uint32_t field;
    uint32_t data[3];
};

struct aml_vdec_cnt_infos
{
    uint32_t bit_rate;
    uint32_t frame_count;
    uint32_t error_frame_count;
    uint32_t drop_frame_count;
    uint32_t total_data;
};

struct aml_dec_params
{
    /* one of V4L2_CONFIG_PARM_DECODE_xxx */
    uint32_t parms_status;
    struct aml_vdec_cfg_infos cfg;
    struct aml_vdec_ps_infos ps;
    struct aml_vdec_hdr_infos hdr;
    struct aml_vdec_cnt_infos cnt;
};

#endif /* __AML_VDEC_H__ */
