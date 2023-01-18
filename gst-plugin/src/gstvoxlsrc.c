/* GStreamer
 * Copyright (C) 2023 FIXME <fixme@example.com>
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
/**
 * SECTION:element-gstvoxlsrc
 *
 * The voxlsrc element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! voxlsrc ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "configuration.h"

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gstvoxlsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_voxlsrc_debug_category);
#define GST_CAT_DEFAULT gst_voxlsrc_debug_category


#define DEFAULT_PROP_DEVICE_NAME        NULL
#define DEFAULT_PROP_DEVICE_FD          -1
#define DEFAULT_PROP_FLAGS              0
#define DEFAULT_PROP_TV_NORM            0

#define GST_TYPE_VOXL_DEVICE_FLAGS (gst_voxl_device_get_type ())

  typedef struct{
    int width;
    int height;
    int fps_n;
    int fps_d;
  }PreferredCapsInfo;




/* prototypes */


static void 
gst_voxlsrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_voxlsrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_voxlsrc_dispose (GObject * object);
static void gst_voxlsrc_finalize (GObject * object);

static GstCaps *gst_voxlsrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_voxlsrc_negotiate (GstBaseSrc * src);
static GstCaps *gst_voxlsrc_fixate (GstBaseSrc * src, GstCaps * caps,  PreferredCapsInfo *pref);
static gboolean gst_voxlsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_voxlsrc_decide_allocation (GstBaseSrc * src,
    GstQuery * query);
static gboolean gst_voxlsrc_start (GstBaseSrc * src);
static gboolean gst_voxlsrc_stop (GstBaseSrc * src);
static void gst_voxlsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_voxlsrc_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_voxlsrc_is_seekable (GstBaseSrc * src);
static gboolean gst_voxlsrc_prepare_seek_segment (GstBaseSrc * src,
    GstEvent * seek, GstSegment * segment);
static gboolean gst_voxlsrc_do_seek (GstBaseSrc * src, GstSegment * segment);
static gboolean gst_voxlsrc_unlock (GstBaseSrc * src);
static gboolean gst_voxlsrc_unlock_stop (GstBaseSrc * src);
static gboolean gst_voxlsrc_query (GstBaseSrc * src, GstQuery * query);
static gboolean gst_voxlsrc_event (GstBaseSrc * src, GstEvent * event);
static GstFlowReturn gst_voxlsrc_create (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** buf);
static GstFlowReturn gst_voxlsrc_alloc (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** buf);
static GstFlowReturn gst_voxlsrc_fill (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer * buf);



enum
{
  PROP_0,
  PROP_DEVICE,           
  PROP_DEVICE_NAME,         
  PROP_DEVICE_FD,           
  PROP_FLAGS,               
  PROP_BRIGHTNESS,          
  PROP_CONTRAST,            
  PROP_SATURATION,          
  PROP_HUE,                 
  PROP_TV_NORM,             
  PROP_IO_MODE,             
  PROP_OUTPUT_IO_MODE,      
  PROP_CAPTURE_IO_MODE,     
  PROP_EXTRA_CONTROLS,      
  PROP_PIXEL_ASPECT_RATIO,  
  PROP_FORCE_ASPECT_RATIO
};

/* signals and args */
enum
{
  SIGNAL_PRE_SET_FORMAT,
  LAST_SIGNAL
};

static guint gst_voxl_signals[LAST_SIGNAL] = { 0 };

typedef enum
{
  GST_VOXL_RAW = 1 << 0,
  GST_VOXL_CODEC = 1 << 1,
  GST_VOXL_TRANSPORT = 1 << 2,
  GST_VOXL_NO_PARSE = 1 << 3,
  GST_VOXL_ALL = 0xffff
} GstVOXLFormatFlags;

typedef struct
{
  guint32 format;
  gboolean dimensions;
  GstVOXLFormatFlags flags;
} GstVOXLFormatDesc;


void
gst_voxl_clear_error (GstVoxlError * voxlerr)
{
  if (voxlerr) {
    g_clear_error (&voxlerr->error);
    g_free (voxlerr->dbg_message);
    voxlerr->dbg_message = NULL;
  }
}

void
gst_voxl_error (gpointer element, GstVoxlError * voxlerr)
{
  GError *error;

  if (!voxlerr || !voxlerr->error)
    return;

  error = voxlerr->error;

  if (error->message)
    GST_WARNING_OBJECT (element, "error: %s", error->message);

  if (voxlerr->dbg_message)
    GST_WARNING_OBJECT (element, "error: %s", voxlerr->dbg_message);

  gst_element_message_full (GST_ELEMENT (element), GST_MESSAGE_ERROR,
      error->domain, error->code, error->message, voxlerr->dbg_message,
      voxlerr->file, voxlerr->func, voxlerr->line);

  error->message = NULL;
  voxlerr->dbg_message = NULL;

  gst_voxl_clear_error (voxlerr);
}

/* pad templates */

static const GstVOXLFormatDesc gst_voxl_formats[] = {
  /* RGB formats */
  {VOXL_PIX_FMT_RGB332, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_ARGB555, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_XRGB555, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_ARGB555XW, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_XRGB555XW, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_RGB565, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_RGB565X, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_BGR666, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_BGR24, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_RGB24, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_ABGR32, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_XBGR32, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_BGRA32, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_BGRX32, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_RGBA32, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_RGBX32, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_ARGB32, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_XRGB32, TRUE, GST_VOXL_RAW},

  /* Deprecated Packed RGB Image Formats (alpha ambiguity) */
  {VOXL_PIX_FMT_RGB444, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_RGB555, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_RGB555X, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_BGR32, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_RGB32, TRUE, GST_VOXL_RAW},

  /* Grey formats */
  {VOXL_PIX_FMT_GREY, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_Y4, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_Y6, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_Y10, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_Y12, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_Y16, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_Y16_BE, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_Y10BPACK, TRUE, GST_VOXL_RAW},

  /* Palette formats */
  {VOXL_PIX_FMT_PAL8, TRUE, GST_VOXL_RAW},

  /* Chrominance formats */
  {VOXL_PIX_FMT_UV8, TRUE, GST_VOXL_RAW},

  /* Luminance+Chrominance formats */
  {VOXL_PIX_FMT_YVU410, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YVU420, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YVU420M, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YUYV, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YYUV, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YVYU, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_UYVY, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_VYUY, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YUV422P, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YUV411P, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_Y41P, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YUV444, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YUV555, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YUV565, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YUV32, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YUV410, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YUV420, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_YUV420M, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_HI240, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_HM12, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_M420, TRUE, GST_VOXL_RAW},

  /* two planes -- one Y, one Cr + Cb interleaved  */
  {VOXL_PIX_FMT_NV12, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV12M, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV12MT, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV12MT_16X16OXL_RAW, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV21, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV21M, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV16, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV16M, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV61, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV61M, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV24, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_NV42, TRUE, GST_VOXL_RAW},

  /* Bayer formats - see http://www.siliconimaging.com/RGB%20Bayer.htm */
  {VOXL_PIX_FMT_SBGGR8, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_SGBRG8, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_SGRBG8, TRUE, GST_VOXL_RAW},
  {VOXL_PIX_FMT_SRGGB8, TRUE, GST_VOXL_RAW},

  /* compressed formats */
  {VOXL_PIX_FMT_MJPEG, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_JPEG, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_PJPG, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_DV, FALSE, GST_VOXL_TRANSPORT},
  {VOXL_PIX_FMT_MPEG, FALSE, GST_VOXL_TRANSPORT},
  {VOXL_PIX_FMT_FWHT, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_H264, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_H264_NO_SC, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_H264_MVC, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_HEVC, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_H263, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_MPEG1, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_MPEG2, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_MPEG4, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_XVID, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_VC1_ANNEX_G, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_VC1_ANNEX_L, FALSE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_VP8, FALSE, GST_VOXL_CODEC | GST_VOXL_NO_PARSE},
  {VOXL_PIX_FMT_VP9, FALSE, GST_VOXL_CODEC | GST_VOXL_NO_PARSE},

  /*  Vendor-specific formats   */
  {VOXL_PIX_FMT_WNVA, TRUE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_SN9C10X, TRUE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_PWC1, TRUE, GST_VOXL_CODEC},
  {VOXL_PIX_FMT_PWC2, TRUE, GST_VOXL_CODEC},
};

#define GST_VOXL_FORMAT_COUNT (G_N_ELEMENTS (gst_voxl_formats))


static GstStaticPadTemplate srcPadTemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw"));


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstVoxlSrc, gst_voxlsrc, GST_TYPE_BASE_SRC,
    GST_DEBUG_CATEGORY_INIT (gst_voxlsrc_debug_category, "voxlsrc", 0,
        "debug category for voxlsrc element"));

/* Add an 'alternate' variant of the caps with the feature */
static void
add_alternate_variant (GstVoxlSrc * voxlsrc, GstCaps * caps,
    GstStructure * structure)
{
  GstStructure *alt_s;

  if (!gst_structure_has_name (structure, "video/x-raw"))
    return;

  alt_s = gst_structure_copy (structure);
  gst_structure_set (alt_s, "interlace-mode", G_TYPE_STRING, "alternate", NULL);

 // gst_caps_append_structure_full (caps, alt_s,
 //     gst_caps_features_new (GST_CAPS_FEATURE_FORMAT_INTERLACED, NULL));
}


static void
gst_voxl_src_parse_fixed_struct (GstStructure * s,
    gint * width, gint * height, gint * fps_n, gint * fps_d)
{
  if (gst_structure_has_field (s, "width") && width)
    gst_structure_get_int (s, "width", width);

  if (gst_structure_has_field (s, "height") && height)
    gst_structure_get_int (s, "height", height);

  if (gst_structure_has_field (s, "framerate") && fps_n && fps_d)
    gst_structure_get_fraction (s, "framerate", fps_n, fps_d);
}

static GstVideoFormat
gst_voxl_object_voxlfourcc_to_video_format (guint32 fourcc)
{
  GstVideoFormat format;

  switch (fourcc) {
    case VOXL_PIX_FMT_GREY:    /*  8  Greyscale     */
      format = GST_VIDEO_FORMAT_GRAY8;
      break;
    case VOXL_PIX_FMT_Y16:
      format = GST_VIDEO_FORMAT_GRAY16_LE;
      break;
    case VOXL_PIX_FMT_Y16_BE:
      format = GST_VIDEO_FORMAT_GRAY16_BE;
      break;
    case VOXL_PIX_FMT_XRGB555:
    case VOXL_PIX_FMT_RGB555:
      format = GST_VIDEO_FORMAT_RGB15;
      break;
    case VOXL_PIX_FMT_XRGB555XW:
    case VOXL_PIX_FMT_RGB555X:
      format = GST_VIDEO_FORMAT_BGR15;
      break;
    case VOXL_PIX_FMT_RGB565:
      format = GST_VIDEO_FORMAT_RGB16;
      break;
    case VOXL_PIX_FMT_RGB24:
      format = GST_VIDEO_FORMAT_RGB;
      break;
    case VOXL_PIX_FMT_BGR24:
      format = GST_VIDEO_FORMAT_BGR;
      break;
    case VOXL_PIX_FMT_XRGB32:
    case VOXL_PIX_FMT_RGB32:
      format = GST_VIDEO_FORMAT_xRGB;
      break;
    case VOXL_PIX_FMT_RGBX32:
      format = GST_VIDEO_FORMAT_RGBx;
      break;
    case VOXL_PIX_FMT_XBGR32:
    case VOXL_PIX_FMT_BGR32:
      format = GST_VIDEO_FORMAT_BGRx;
      break;
    case VOXL_PIX_FMT_BGRX32:
      format = GST_VIDEO_FORMAT_xBGR;
      break;
    case VOXL_PIX_FMT_ABGR32:
      format = GST_VIDEO_FORMAT_BGRA;
      break;
    case VOXL_PIX_FMT_BGRA32:
      format = GST_VIDEO_FORMAT_ABGR;
      break;
    case VOXL_PIX_FMT_RGBA32:
      format = GST_VIDEO_FORMAT_RGBA;
      break;
    case VOXL_PIX_FMT_ARGB32:
      format = GST_VIDEO_FORMAT_ARGB;
      break;
    case VOXL_PIX_FMT_NV12:
    case VOXL_PIX_FMT_NV12M:
      format = GST_VIDEO_FORMAT_NV12;
      break;
    case VOXL_PIX_FMT_NV12MT:
      format = GST_VIDEO_FORMAT_NV12_64Z32;
      break;
    case VOXL_PIX_FMT_NV21:
    case VOXL_PIX_FMT_NV21M:
      format = GST_VIDEO_FORMAT_NV21;
      break;
    case VOXL_PIX_FMT_YVU410:
      format = GST_VIDEO_FORMAT_YVU9;
      break;
    case VOXL_PIX_FMT_YUV410:
      format = GST_VIDEO_FORMAT_YUV9;
      break;
    case VOXL_PIX_FMT_YUV420:
    case VOXL_PIX_FMT_YUV420M:
      format = GST_VIDEO_FORMAT_I420;
      break;
    case VOXL_PIX_FMT_YUYV:
      format = GST_VIDEO_FORMAT_YUY2;
      break;
    case VOXL_PIX_FMT_YVU420:
      format = GST_VIDEO_FORMAT_YV12;
      break;
    case VOXL_PIX_FMT_UYVY:
      format = GST_VIDEO_FORMAT_UYVY;
      break;
    case VOXL_PIX_FMT_YUV411P:
      format = GST_VIDEO_FORMAT_Y41B;
      break;
    case VOXL_PIX_FMT_YUV422P:
      format = GST_VIDEO_FORMAT_Y42B;
      break;
    case VOXL_PIX_FMT_YVYU:
      format = GST_VIDEO_FORMAT_YVYU;
      break;
    case VOXL_PIX_FMT_NV16:
    case VOXL_PIX_FMT_NV16M:
      format = GST_VIDEO_FORMAT_NV16;
      break;
    case VOXL_PIX_FMT_NV61:
    case VOXL_PIX_FMT_NV61M:
      format = GST_VIDEO_FORMAT_NV61;
      break;
    case VOXL_PIX_FMT_NV24:
      format = GST_VIDEO_FORMAT_NV24;
      break;
    default:
      format = GST_VIDEO_FORMAT_UNKNOWN;
      break;
  }

  return format;
}

void
gst_voxl_object_install_properties_helper (GObjectClass * gobject_class,
    const char *default_device)
{
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device", "Device location",
          default_device, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE_NAME,
      g_param_spec_string ("device-name", "Device name",
          "Name of the device", DEFAULT_PROP_DEVICE_NAME,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DEVICE_FD,
      g_param_spec_int ("device-fd", "File descriptor",
          "File descriptor of the device", -1, G_MAXINT, DEFAULT_PROP_DEVICE_FD,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}



/* TODO Consider framerate */
static gint
gst_voxl_fixed_caps_compare (GstCaps * caps_a, GstCaps * caps_b,
    PreferredCapsInfo *pref)
{
  GstStructure *a, *b;
  gint aw = G_MAXINT, ah = G_MAXINT, ad = G_MAXINT;
  gint bw = G_MAXINT, bh = G_MAXINT, bd = G_MAXINT;
  gint ret;

  a = gst_caps_get_structure (caps_a, 0);
  b = gst_caps_get_structure (caps_b, 0);

  gst_voxl_src_parse_fixed_struct (a, &aw, &ah, NULL, NULL);
  gst_voxl_src_parse_fixed_struct (b, &bw, &bh, NULL, NULL);

  /* When both are smaller then pref, just append to the end */
  if ((bw < pref->width || bh < pref->height)
      && (aw < pref->width || ah < pref->height)) {
    ret = 1;
    goto done;
  }

  /* If a is smaller then pref and not b, then a goes after b */
  if (aw < pref->width || ah < pref->height) {
    ret = 1;
    goto done;
  }

  /* If b is smaller then pref and not a, then a goes before b */
  if (bw < pref->width || bh < pref->height) {
    ret = -1;
    goto done;
  }

  /* Both are larger or equal to the preference, prefer the smallest */
  ad = MAX (1, aw - pref->width) * MAX (1, ah - pref->height);
  bd = MAX (1, bw - pref->width) * MAX (1, bh - pref->height);

  /* Adjust slightly in case width/height matched the preference */
  if (aw == pref->width)
    ad -= 1;

  if (ah == pref->height)
    ad -= 1;

  if (bw == pref->width)
    bd -= 1;

  if (bh == pref->height)
    bd -= 1;

  /* If the choices are equivalent, maintain the order */
  if (ad == bd)
    ret = 1;
  else
    ret = ad - bd;

done:
  GST_TRACE ("Placing %ix%i (%s) %s %ix%i (%s)", aw, ah,
      gst_structure_get_string (a, "format"), ret > 0 ? "after" : "before", bw,
      bh, gst_structure_get_string (b, "format"));
  return ret;
}

static GstStructure *
gst_voxl_object_voxlfourcc_to_bare_struct (guint32 fourcc)
{
  GstStructure *structure = NULL;

  switch (fourcc) {
    case VOXL_PIX_FMT_MJPEG:   /* Motion-JPEG */
    case VOXL_PIX_FMT_PJPG:    /* Progressive-JPEG */
    case VOXL_PIX_FMT_JPEG:    /* JFIF JPEG */
      structure = gst_structure_new_empty ("image/jpeg");
      break;
    case VOXL_PIX_FMT_MPEG1:
      structure = gst_structure_new ("video/mpeg",
          "mpegversion", G_TYPE_INT, 1, NULL);
      break;
    case VOXL_PIX_FMT_MPEG2:
      structure = gst_structure_new ("video/mpeg",
          "mpegversion", G_TYPE_INT, 2, NULL);
      break;
    case VOXL_PIX_FMT_MPEG4:
    case VOXL_PIX_FMT_XVID:
      structure = gst_structure_new ("video/mpeg",
          "mpegversion", G_TYPE_INT, 4, "systemstream",
          G_TYPE_BOOLEAN, FALSE, NULL);
      break;
    case VOXL_PIX_FMT_FWHT:
      structure = gst_structure_new_empty ("video/x-fwht");
      break;
    case VOXL_PIX_FMT_H263:
      structure = gst_structure_new ("video/x-h263",
          "variant", G_TYPE_STRING, "itu", NULL);
      break;
    case VOXL_PIX_FMT_H264:    /* H.264 */
      structure = gst_structure_new ("video/x-h264",
          "stream-format", G_TYPE_STRING, "byte-stream", "alignment",
          G_TYPE_STRING, "au", NULL);
      break;
    case VOXL_PIX_FMT_H264_NO_SC:
      structure = gst_structure_new ("video/x-h264",
          "stream-format", G_TYPE_STRING, "avc", "alignment",
          G_TYPE_STRING, "au", NULL);
      break;
    case VOXL_PIX_FMT_HEVC:    /* H.265 */
      structure = gst_structure_new ("video/x-h265",
          "stream-format", G_TYPE_STRING, "byte-stream", "alignment",
          G_TYPE_STRING, "au", NULL);
      break;
    case VOXL_PIX_FMT_VC1_ANNEX_G:
    case VOXL_PIX_FMT_VC1_ANNEX_L:
      structure = gst_structure_new ("video/x-wmv",
          "wmvversion", G_TYPE_INT, 3, "format", G_TYPE_STRING, "WVC1", NULL);
      break;
    case VOXL_PIX_FMT_VP8:
      structure = gst_structure_new_empty ("video/x-vp8");
      break;
    case VOXL_PIX_FMT_VP9:
      structure = gst_structure_new_empty ("video/x-vp9");
      break;
    case VOXL_PIX_FMT_GREY:    /*  8  Greyscale     */
    case VOXL_PIX_FMT_Y16:
    case VOXL_PIX_FMT_Y16_BE:
    case VOXL_PIX_FMT_XRGB555:
    case VOXL_PIX_FMT_RGB555:
    case VOXL_PIX_FMT_XRGB555XW:
    case VOXL_PIX_FMT_RGB555X:
    case VOXL_PIX_FMT_RGB565:
    case VOXL_PIX_FMT_RGB24:
    case VOXL_PIX_FMT_BGR24:
    case VOXL_PIX_FMT_RGB32:
    case VOXL_PIX_FMT_XRGB32:
    case VOXL_PIX_FMT_ARGB32:
    case VOXL_PIX_FMT_RGBX32:
    case VOXL_PIX_FMT_RGBA32:
    case VOXL_PIX_FMT_BGR32:
    case VOXL_PIX_FMT_BGRX32:
    case VOXL_PIX_FMT_BGRA32:
    case VOXL_PIX_FMT_XBGR32:
    case VOXL_PIX_FMT_ABGR32:
    case VOXL_PIX_FMT_NV12:    /* 12  Y/CbCr 4:2:0  */
    case VOXL_PIX_FMT_NV12M:
    case VOXL_PIX_FMT_NV12MT:
    case VOXL_PIX_FMT_NV21:    /* 12  Y/CrCb 4:2:0  */
    case VOXL_PIX_FMT_NV21M:
    case VOXL_PIX_FMT_NV16:    /* 16  Y/CbCr 4:2:2  */
    case VOXL_PIX_FMT_NV16M:
    case VOXL_PIX_FMT_NV61:    /* 16  Y/CrCb 4:2:2  */
    case VOXL_PIX_FMT_NV61M:
    case VOXL_PIX_FMT_NV24:    /* 24  Y/CrCb 4:4:4  */
    case VOXL_PIX_FMT_YVU410:
    case VOXL_PIX_FMT_YUV410:
    case VOXL_PIX_FMT_YUV420:  /* I420/IYUV */
    case VOXL_PIX_FMT_YUV420M:
    case VOXL_PIX_FMT_YUYV:
    case VOXL_PIX_FMT_YVU420:
    case VOXL_PIX_FMT_UYVY:
    case VOXL_PIX_FMT_YUV422P:
    case VOXL_PIX_FMT_YVYU:
    case VOXL_PIX_FMT_YUV411P:{
      GstVideoFormat format;
      format = gst_voxl_object_voxlfourcc_to_video_format (fourcc);
      if (format != GST_VIDEO_FORMAT_UNKNOWN)
        structure = gst_structure_new ("video/x-raw",
            "format", G_TYPE_STRING, gst_video_format_to_string (format), NULL);
      break;
    }
    case VOXL_PIX_FMT_DV:
      structure =
          gst_structure_new ("video/x-dv", "systemstream", G_TYPE_BOOLEAN, TRUE,
          NULL);
      break;
    case VOXL_PIX_FMT_MPEG:    /* MPEG          */
      structure = gst_structure_new ("video/mpegts",
          "systemstream", G_TYPE_BOOLEAN, TRUE, NULL);
      break;
    case VOXL_PIX_FMT_WNVA:    /* Winnov hw compress */
      break;
    case VOXL_PIX_FMT_SBGGR8:
    case VOXL_PIX_FMT_SGBRG8:
    case VOXL_PIX_FMT_SGRBG8:
    case VOXL_PIX_FMT_SRGGB8:
      structure = gst_structure_new ("video/x-bayer", "format", G_TYPE_STRING,
          fourcc == VOXL_PIX_FMT_SBGGR8 ? "bggr" :
          fourcc == VOXL_PIX_FMT_SGBRG8 ? "gbrg" :
          fourcc == VOXL_PIX_FMT_SGRBG8 ? "grbg" :
          /* fourcc == VOXL_PIX_FMT_SRGGB8 ? */ "rggb", NULL);
      break;
    case VOXL_PIX_FMT_SN9C10X:
      structure = gst_structure_new_empty ("video/x-sonix");
      break;
    case VOXL_PIX_FMT_PWC1:
      structure = gst_structure_new_empty ("video/x-pwc1");
      break;
    case VOXL_PIX_FMT_PWC2:
      structure = gst_structure_new_empty ("video/x-pwc2");
      break;
    case VOXL_PIX_FMT_RGB332:
    case VOXL_PIX_FMT_BGR666:
    case VOXL_PIX_FMT_ARGB555XW:
    case VOXL_PIX_FMT_RGB565X:
    case VOXL_PIX_FMT_RGB444:
    case VOXL_PIX_FMT_YYUV:    /* 16  YUV 4:2:2     */
    case VOXL_PIX_FMT_HI240:   /*  8  8-bit color   */
    case VOXL_PIX_FMT_Y4:
    case VOXL_PIX_FMT_Y6:
    case VOXL_PIX_FMT_Y10:
    case VOXL_PIX_FMT_Y12:
    case VOXL_PIX_FMT_Y10BPACK:
    case VOXL_PIX_FMT_YUV444:
    case VOXL_PIX_FMT_YUV555:
    case VOXL_PIX_FMT_YUV565:
    case VOXL_PIX_FMT_Y41P:
    case VOXL_PIX_FMT_YUV32:
    case VOXL_PIX_FMT_NV12MT_16X16OXL_RAW:
    case VOXL_PIX_FMT_NV42:
    case VOXL_PIX_FMT_H264_MVC:
    default:
      GST_DEBUG ("Unsupported fourcc 0x%08x %" GST_FOURCC_FORMAT,
          fourcc, GST_FOURCC_ARGS (fourcc));
      break;
  }

  return structure;
}

static GstCaps *
gst_voxl_object_get_caps_helper (GstVOXLFormatFlags flags)
{
  GstStructure *structure;
  GstCaps *caps, *caps_interlaced;
  guint i;

  caps = gst_caps_new_empty ();
  caps_interlaced = gst_caps_new_empty ();
  for (i = 0; i < GST_VOXL_FORMAT_COUNT; i++) {

    if ((gst_voxl_formats[i].flags & flags) == 0)
      continue;

    structure =
        gst_voxl_object_voxlfourcc_to_bare_struct (gst_voxl_formats[i].format);

    if (structure) {
      GstStructure *alt_s = NULL;

      if (gst_voxl_formats[i].dimensions) {
        gst_structure_set (structure,
            "width", GST_TYPE_INT_RANGE, 1, GST_VOXL_MAX_SIZE,
            "height", GST_TYPE_INT_RANGE, 1, GST_VOXL_MAX_SIZE,
            "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
      }

      switch (gst_voxl_formats[i].format) {
        case VOXL_PIX_FMT_RGB32:
          alt_s = gst_structure_copy (structure);
          gst_structure_set (alt_s, "format", G_TYPE_STRING, "ARGB", NULL);
          break;
        case VOXL_PIX_FMT_BGR32:
          alt_s = gst_structure_copy (structure);
          gst_structure_set (alt_s, "format", G_TYPE_STRING, "BGRA", NULL);
        default:
          break;
      }

      gst_caps_append_structure (caps, structure);

      if (alt_s) {
        gst_caps_append_structure (caps, alt_s);
        add_alternate_variant (NULL, caps_interlaced, alt_s);
      }

      add_alternate_variant (NULL, caps_interlaced, structure);
    }
  }

  caps = gst_caps_simplify (caps);
  caps_interlaced = gst_caps_simplify (caps_interlaced);

  return gst_caps_merge (caps, caps_interlaced);
}

GstCaps *
gst_voxl_object_get_all_caps (void)
{
  static GstCaps *caps = NULL;

  if (g_once_init_enter (&caps)) {
    GstCaps *all_caps = gst_voxl_object_get_caps_helper (GST_VOXL_ALL);
    GST_MINI_OBJECT_FLAG_SET (all_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
    g_once_init_leave (&caps, all_caps);
  }

  return caps;
}


GstPadTemplate* padTemplate;

static void
gst_voxlsrc_class_init (GstVoxlSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);


  gobject_class->set_property = gst_voxlsrc_set_property;
  gobject_class->get_property = gst_voxlsrc_get_property;

  gst_voxl_object_install_properties_helper(gobject_class, VOXL_DEFAULT_DEVICE_PROP);

  gobject_class->dispose = gst_voxlsrc_dispose;
  gobject_class->finalize = gst_voxlsrc_finalize;


  gst_voxl_signals[SIGNAL_PRE_SET_FORMAT] = g_signal_new ("prepare-format",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_INT, GST_TYPE_CAPS);
    
  gst_element_class_set_static_metadata (element_class,
      "Voxl Source", "Source/Video",
      "Reads frames from a Voxl camera device",
      "Amit Hershkovich <amit.hershkovich@xtend.me>");

  padTemplate = gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_voxl_object_get_all_caps());

  gst_element_class_add_pad_template(element_class, padTemplate);

  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_voxlsrc_get_caps);
  base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_voxlsrc_negotiate);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_voxlsrc_fixate);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_voxlsrc_set_caps);
  base_src_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_voxlsrc_decide_allocation);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_voxlsrc_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_voxlsrc_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_voxlsrc_get_times);
  base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_voxlsrc_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_voxlsrc_is_seekable);
  base_src_class->prepare_seek_segment =
      GST_DEBUG_FUNCPTR (gst_voxlsrc_prepare_seek_segment);
  base_src_class->do_seek = GST_DEBUG_FUNCPTR (gst_voxlsrc_do_seek);
  base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_voxlsrc_unlock);
  base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_voxlsrc_unlock_stop);
  base_src_class->query = GST_DEBUG_FUNCPTR (gst_voxlsrc_query);
  base_src_class->event = GST_DEBUG_FUNCPTR (gst_voxlsrc_event);

  base_src_class->create = GST_DEBUG_FUNCPTR (gst_voxlsrc_create);
  base_src_class->alloc = GST_DEBUG_FUNCPTR (gst_voxlsrc_alloc);
  base_src_class->fill = GST_DEBUG_FUNCPTR (gst_voxlsrc_fill);

}

static void
gst_voxlsrc_init (GstVoxlSrc * voxlsrc)
{
    voxlsrc->pool = gst_buffer_pool_new();
    guint64 size_format;

    gst_voxlsrc_get_size(voxlsrc, &size_format);
      GstCaps *caps = gst_caps_new_simple ("video/x-raw",
     "format", G_TYPE_STRING, "NV12",
     "framerate", GST_TYPE_FRACTION, 30, 1,
     "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
     "width", G_TYPE_INT, 640,
     "height", G_TYPE_INT, 480,
     NULL);

    GstStructure* structure = gst_buffer_pool_get_config (voxlsrc->pool);

    gst_buffer_pool_config_set_params (structure, caps, size_format, 50, 100);
    gst_buffer_pool_set_config(voxlsrc->pool, structure);
    if (gst_buffer_pool_set_active(voxlsrc->pool, TRUE) != TRUE)
    {
      M_PRINT("Failed to create pool");
    }

    voxlsrc->m_queue = g_async_queue_new();


    /* pad through which data goes out of the element */
    voxlsrc->srcpad = gst_pad_new_from_template (padTemplate, "src");


}


void
gst_voxlsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (object);
  M_PRINT("Proprety is object %s \n", g_value_get_string(value));
  gchar* device_name;
  switch (property_id) {
    case PROP_DEVICE:

      device_name = g_value_get_string(value);

      if (g_strcmp0(device_name, VOXL_DEVICE_TRACKER_CAMERA_PROP) == 0)
      {
        M_PRINT("Tracker Camera founded\n");
        voxlsrc->device_channel = VOXL_DEVICE_TRACKER_CAMERA_CHANNEL;
        memcpy(voxlsrc->device_name,device_name,sizeof(device_name));
      }

      else if (g_strcmp0(device_name, VOXL_DEVICE_HIRES_CAMERA_PROP) == 0)
      {
        M_PRINT("Hires Camera founded\n");
        voxlsrc->device_channel = VOXL_DEVICE_HIRES_CAMERA_CHANNEL;
        memcpy(voxlsrc->device_name,device_name,sizeof(device_name));
      }

      else 
      {
        M_PRINT("Error in set device property");
      }
      
      break;
    case PROP_DEVICE_NAME:
      break;
    case PROP_DEVICE_FD:

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }

}

void
gst_voxlsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (object);

  switch (property_id) {
    case PROP_DEVICE:
        g_value_set_int(value, voxlsrc->device_prop);
        break; 
      break;
    case PROP_DEVICE_NAME:
      break;
    case PROP_DEVICE_FD:

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

void
gst_voxlsrc_dispose (GObject * object)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (object);

  GST_DEBUG_OBJECT (voxlsrc, "dispose");

  /* clean up as possible.  may be called multiple times */

}

void
gst_voxlsrc_finalize (GObject * object)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (object);

  GST_DEBUG_OBJECT (voxlsrc, "finalize");

  /* clean up object here */

}



/* get caps from subclass */
static GstCaps *
gst_voxlsrc_get_caps (GstBaseSrc * src, GstCaps * filter)
{

  GstVoxlSrc *voxlsrc;

  voxlsrc = GST_VOXLSRC (src);

  return gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (voxlsrc));

}


static GstCaps *
gst_voxlsrc_fixate (GstBaseSrc * basesrc, GstCaps * caps,
    PreferredCapsInfo *pref)
{

  M_PRINT("FIXATE \n");

  GstVoxlSrc *voxlsrc = GST_VOXLSRC (basesrc);

  GstCaps *fcaps = NULL;

  return fcaps;
}


/* decide on caps */
static gboolean
gst_voxlsrc_negotiate (GstBaseSrc * src)
{

  M_PRINT("negotiate \n");

  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);
  return TRUE;
}


/* notify the subclass of new caps */
static gboolean
gst_voxlsrc_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  M_PRINT("CAPS SET \n");

  GST_DEBUG_OBJECT (voxlsrc, "set_caps");

  return TRUE;
}

/* setup allocation query */
static gboolean
gst_voxlsrc_decide_allocation (GstBaseSrc * src, GstQuery * query)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);
  
  M_PRINT("decide_allocation");

  GST_DEBUG_OBJECT (voxlsrc, "decide_allocation");

  return TRUE;
}


// camera helper callback whenever a frame arrives
static void _cam_helper_cb(
    __attribute__((unused))int ch,
                           camera_image_metadata_t meta,
                           char* frame,
                           void* src)
{

  GstMapInfo info;
  GstVoxlSrc *voxlsrc = (GstVoxlSrc*) src;
  context_data* ctx = &(voxlsrc->data);
  GstBuffer* gst_buffer;
  gst_buffer = gst_buffer_new_allocate(NULL, meta.size_bytes, NULL);

  gst_buffer_map(gst_buffer, &info, GST_MAP_WRITE);

  memcpy(info.data, frame, meta.size_bytes);

    // GST_BUFFER_TIMESTAMP(gst_buffer) = ((guint64) meta.timestamp_ns - ctx->initial_timestamp);
    // GST_BUFFER_DURATION(gst_buffer) = ((guint64) meta.timestamp_ns) - ctx->last_timestamp;
  g_async_queue_push (voxlsrc->m_queue, gst_buffer);







  //   if (info.size < ctx->input_frame_size) {
  //       M_ERROR("Not enough memory for the frame buffer\n");
  //       main_running = 0;
  //       return;
  //   }

  //   memcpy(info.data, frame, ctx->input_frame_size);

  //   // To get minimal latency make sure to set this to the timestamp of
  //   // the very first frame that we will be sending through the pipeline.
  //   if (ctx->initial_timestamp == 0) ctx->initial_timestamp = (guint64) meta.timestamp_ns;

  //   // Do the timestamp calculations.
  //   // It is very important to set these up accurately.
  //   // Otherwise, the stream can look bad or just not work at all.
  //   // TODO: Experiment with taking some time off of pts???
  //   GST_BUFFER_TIMESTAMP(gst_buffer) = ((guint64) meta.timestamp_ns - ctx->initial_timestamp);
  //   GST_BUFFER_DURATION(gst_buffer) = ((guint64) meta.timestamp_ns) - ctx->last_timestamp;
  //   ctx->last_timestamp = (guint64) meta.timestamp_ns;
  //   pthread_mutex_unlock(&ctx->lock);


  //   ctx->output_frame_number++;

  //   // Signal that the frame is ready for use
  //   //g_signal_emit_by_name(ctx->app_source, "push-buffer", gst_buffer, &status);


  //   //status = gst_pad_push (voxlsrc->srcpad, gst_buffer);
  //   //voxlsrc->gst_buffer = gst_buffer;


  //   ctx->input_frame_number++;

  //   // Release the buffer so that we don't have a memory leak
  //   gst_buffer_unmap(gst_buffer, &info);
  //   gst_buffer_unref(gst_buffer);

}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_voxlsrc_start (GstBaseSrc * src)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);
  gchar* name_device;

    // Have the configuration module fill in the context data structure
    // with all of the required parameters to support the given configuration
    // in the given configuration file.
    if (prepare_configuration(&voxlsrc->data)) {
        M_ERROR("Could not parse the configuration data\n");
        return -1;
    }

    // start signal handler so we can exit cleanly
    if(enable_signal_handler()==-1){
        M_ERROR("Failed to start signal handler\n");
        return -1;
    }

  // if (voxlsrc->device_prop == VOXL_DEVICE_HIGHRES_CAMERA)
  // {
  //   name_device = VOXL_DEVICE_HIGHRES_CAMERA_PROP;
  // }

  // else 
  // {
  //   name_device = VOXL_DEVICE_TRACKER_CAMERA_PROP;
  // }

  pipe_client_set_camera_helper_cb(voxlsrc->device_channel, _cam_helper_cb, voxlsrc);

  pipe_client_open(voxlsrc->device_channel, voxlsrc->device_name, "xtra-voxl-src", EN_PIPE_CLIENT_CAMERA_HELPER, 0);

  M_PRINT("STARTING CAMERA  %s .......\n", voxlsrc->device_name);

  GST_DEBUG_OBJECT (voxlsrc, "start");

  return TRUE;
}

static gboolean
gst_voxlsrc_stop (GstBaseSrc * src)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  // Wait for the buffer processing thread to exit
  pipe_client_close_all();

  GST_DEBUG_OBJECT (voxlsrc, "stop");

  return TRUE;
}

/* given a buffer, return start and stop time when it should be pushed
 * out. The base class will sync on the clock using these times. */
static void
gst_voxlsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  //M_PRINT("Times");

  GST_DEBUG_OBJECT (voxlsrc, "get_times");

}

/* get the total size of the resource in bytes */
static gboolean
gst_voxlsrc_get_size (GstBaseSrc * src, guint64 * size)
{

  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  size = (guint64) (640.0f* 480.0f * 1.50f);

  GST_DEBUG_OBJECT (voxlsrc, "get_size");

  return TRUE;
}

/* check if the resource is seekable */
static gboolean
gst_voxlsrc_is_seekable (GstBaseSrc * src)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  GST_DEBUG_OBJECT (voxlsrc, "is_seekable");

  return TRUE;
}

/* Prepare the segment on which to perform do_seek(), converting to the
 * current basesrc format. */
static gboolean
gst_voxlsrc_prepare_seek_segment (GstBaseSrc * src, GstEvent * seek,
    GstSegment * segment)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  GST_DEBUG_OBJECT (voxlsrc, "prepare_seek_segment");

  return TRUE;
}

/* notify subclasses of a seek */
static gboolean
gst_voxlsrc_do_seek (GstBaseSrc * src, GstSegment * segment)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  GST_DEBUG_OBJECT (voxlsrc, "do_seek");

  return TRUE;
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
static gboolean
gst_voxlsrc_unlock (GstBaseSrc * src)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  GST_DEBUG_OBJECT (voxlsrc, "unlock");

  return TRUE;
}

/* Clear any pending unlock request, as we succeeded in unlocking */
static gboolean
gst_voxlsrc_unlock_stop (GstBaseSrc * src)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  GST_DEBUG_OBJECT (voxlsrc, "unlock_stop");

  return TRUE;
}

/* notify subclasses of a query */
static gboolean
gst_voxlsrc_query (GstBaseSrc * bsrc, GstQuery * query)
{

  GstVoxlSrc *src;
  gboolean res = FALSE;

  src = GST_VOXLSRC (bsrc);
  switch (GST_QUERY_TYPE (query)) {

    case GST_QUERY_LATENCY:
      gst_query_set_latency (query, TRUE, 30, 30);
      M_PRINT("GST_QUERY_LATENCY \n");
      break;

    case GST_QUERY_CAPS:
      gst_query_set_caps_result(query, gst_pad_template_get_caps(padTemplate));
      M_PRINT("GST_QUERY_CAPS \n");
      break;

    case GST_QUERY_URI:
      M_PRINT("GST_QUERY_URI \n");
      //gst_query_set_uri(query, "URI");
      break;

    case GST_QUERY_DURATION:
        M_PRINT("GST_QUERY_DURATION \n");
        break;

    case GST_QUERY_SEEKING:
        M_PRINT("GST_QUERY_SEEKING \n");
        break;

    default:
      M_PRINT("Deafult query  %d\n", GST_QUERY_TYPE (query));
      break;
  }

done:

  return res;
}

/* notify subclasses of an event */
static gboolean
gst_voxlsrc_event (GstBaseSrc * src, GstEvent * event)
{
  gboolean ret;
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);


  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
      M_PRINT("event -  GST_EVENT_CAPS \n");

      break;
    case GST_EVENT_EOS:
      M_PRINT("event -  GST_EVENT_EOS \n");
      /* end-of-stream, we should close down all stream leftovers here */
      //gst_voxlsrc_stop(voxlsrc);

      break;

    case GST_EVENT_LATENCY:
      M_PRINT("event -  GST_EVENT_LATENCY \n");
      break;
    default:
      /* just call the default handler */
      // ret = gst_pad_event_default (voxlsrc->srcpad, event);
    break;
  }
  return TRUE;

  GST_DEBUG_OBJECT (voxlsrc, "event");

  return TRUE;
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn
gst_voxlsrc_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);
  GstFlowReturn ret;

  gint sizeQueue = g_async_queue_length(voxlsrc->m_queue);

  *buf  = g_async_queue_timeout_pop(voxlsrc->m_queue, 0xFFFFFFF);

  return GST_FLOW_OK;

}

/* ask the subclass to allocate an output buffer. The default implementation
 * will use the negotiated allocator. */
static GstFlowReturn
gst_voxlsrc_alloc (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  *buf = gst_buffer_new_allocate(NULL, size, NULL);

  gst_voxlsrc_fill(src, offset, size, *buf);
  
  return GST_FLOW_OK;
}

/* ask the subclass to fill the buffer with data from offset and size */
static GstFlowReturn
gst_voxlsrc_fill (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer * buf)
{
  GstVoxlSrc *voxlsrc = GST_VOXLSRC (src);

  //M_PRINT("Please fill buffer with data size  %d\n", size);

  GST_DEBUG_OBJECT (voxlsrc, "fill");

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

  /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register (plugin, "voxlsrc", GST_RANK_NONE,
      GST_TYPE_VOXLSRC);
}

/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "1.0"
#endif
#ifndef PACKAGE
#define PACKAGE "voxl_package"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "Voxl"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http:///"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    voxlsrc,
    "FIXME plugin description",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
