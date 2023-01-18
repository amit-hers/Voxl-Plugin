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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_VOXLSRC_H_
#define _GST_VOXLSRC_H_

#include <gst/base/gstbasesrc.h>

#include <gst/video/video.h>
#include <gst/video/video-format.h>
#include <gst/gst.h>


#include <modal_pipe.h>
#include <modal_journal.h>

#include "context.h"

G_BEGIN_DECLS

#define GST_TYPE_VOXLSRC   (gst_voxlsrc_get_type())
#define GST_VOXLSRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VOXLSRC,GstVoxlSrc))
#define GST_VOXLSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VOXLSRC,GstVoxlSrcClass))
#define GST_IS_VOXLSRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VOXLSRC))
#define GST_IS_VOXLSRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VOXLSRC))

typedef struct _GstVoxlSrc GstVoxlSrc;
typedef struct _GstVoxlSrcClass GstVoxlSrcClass;
typedef struct _GstVoxlBufferPool GstVoxlBufferPool;

#define GST_VOXL_MAX_SIZE (1<<15) /* 2^15 == 32768 */
#define VIDEO_MAX_FRAME               32
#define GST_VOXL_FLOW_RESOLUTION_CHANGE GST_FLOW_CUSTOM_SUCCESS_2
#define GST_VOXL_FLOW_LAST_BUFFER GST_FLOW_CUSTOM_SUCCESS
#define GST_VOXL_FLOW_CORRUPTED_BUFFER GST_FLOW_CUSTOM_SUCCESS_1

#define VOXL_DEVICE_HIRES_CAMERA_PROP "hires"
#define VOXL_DEVICE_TRACKER_CAMERA_PROP "tracking"
#define VOXL_DEFAULT_DEVICE_PROP VOXL_DEVICE_HIRES_CAMERA_PROP

typedef enum 
{
  VOXL_DEVICE_HIRES_CAMERA_CHANNEL,
  VOXL_DEVICE_TRACKER_CAMERA_CHANNEL
}VoxlCameraDeviceChannel;

#define VOXL_MAX_SIZE_PROP 30

enum voxl_buf_type {
	VOXL_BUF_TYPE_VIDEO_CAPTURE        = 1,
	VOXL_BUF_TYPE_VIDEO_OUTPUT         = 2,
	VOXL_BUF_TYPE_VIDEO_OVERLAY        = 3,
	VOXL_BUF_TYPE_VBI_CAPTURE          = 4,
	VOXL_BUF_TYPE_VBI_OUTPUT           = 5,
	VOXL_BUF_TYPE_SLICED_VBI_CAPTURE   = 6,
	VOXL_BUF_TYPE_SLICED_VBI_OUTPUT    = 7,
	VOXL_BUF_TYPE_VIDEO_OUTPUT_OVERLAY = 8,
	VOXL_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 9,
	VOXL_BUF_TYPE_VIDEO_OUTPUT_MPLANE  = 10,
	VOXL_BUF_TYPE_SDR_CAPTURE          = 11,
	VOXL_BUF_TYPE_SDR_OUTPUT           = 12,
	VOXL_BUF_TYPE_META_CAPTURE         = 13,
	VOXL_BUF_TYPE_META_OUTPUT	   = 14,
	/* Deprecated, do not use */
	VOXL_BUF_TYPE_PRIVATE              = 0x80,
};


#define VOXL_TYPE_IS_OUTPUT(type)				\
	((type) == VOXL_BUF_TYPE_VIDEO_OUTPUT			\
	 || (type) == VOXL_BUF_TYPE_VIDEO_OUTPUT_MPLANE		\
	 || (type) == VOXL_BUF_TYPE_VIDEO_OVERLAY		\
	 || (type) == VOXL_BUF_TYPE_VIDEO_OUTPUT_OVERLAY	\
	 || (type) == VOXL_BUF_TYPE_VBI_OUTPUT			\
	 || (type) == VOXL_BUF_TYPE_SLICED_VBI_OUTPUT		\
	 || (type) == VOXL_BUF_TYPE_SDR_OUTPUT			\
	 || (type) == VOXL_BUF_TYPE_META_OUTPUT)



/* Control classes */
#define VOXL_CTRL_CLASS_USER		0x00980000	/* Old-style 'user' controls */
#define VOXL_CTRL_CLASS_MPEG		0x00990000	/* MPEG-compression controls */
#define VOXL_CTRL_CLASS_CAMERA		0x009a0000	/* Camera class controls */
#define VOXL_CTRL_CLASS_FM_TX		0x009b0000	/* FM Modulator controls */
#define VOXL_CTRL_CLASS_FLASH		0x009c0000	/* Camera flash controls */
#define VOXL_CTRL_CLASS_JPEG		0x009d0000	/* JPEG-compression controls */
#define VOXL_CTRL_CLASS_IMAGE_SOURCE	0x009e0000	/* Image source controls */
#define VOXL_CTRL_CLASS_IMAGE_PROC	0x009f0000	/* Image processing controls */
#define VOXL_CTRL_CLASS_DV		0x00a00000	/* Digital Video controls */
#define VOXL_CTRL_CLASS_FM_RX		0x00a10000	/* FM Receiver controls */
#define VOXL_CTRL_CLASS_RF_TUNER	0x00a20000	/* RF tuner controls */
#define VOXL_CTRL_CLASS_DETECT		0x00a30000	/* Detection controls OXL
/* User-class control IDs */

#define VOXL_CID_BASE			(VOXL_CTRL_CLASS_USER | 0x900)
#define VOXL_CID_USER_BASE		VOXL_CID_BASE
#define VOXL_CID_USER_CLASS		(VOXL_CTRL_CLASS_USER | 1)
#define VOXL_CID_BRIGHTNESS		(VOXL_CID_BASE+0)
#define VOXL_CID_CONTRAST		(VOXL_CID_BASE+1)
#define VOXL_CID_SATURATION		(VOXL_CID_BASE+2)
#define VOXL_CID_HUE			(VOXL_CID_BASE+3)
#define VOXL_CID_AUDIO_VOLUME		(VOXL_CID_BASE+5)
#define VOXL_CID_AUDIO_BALANCE		(VOXL_CID_BASE+6)
#define VOXL_CID_AUDIO_BASS		(VOXL_CID_BASE+7)
#define VOXL_CID_AUDIO_TREBLE		(VOXL_CID_BASE+8)
#define VOXL_CID_AUDIO_MUTE		(VOXL_CID_BASE+9)
#define VOXL_CID_AUDIO_LOUDNESS		(VOXL_CID_BASE+10)
#define VOXL_CID_BLACK_LEVEL		(VOXL_CID_BASE+11) /* Deprecated */
#define VOXL_CID_AUTO_WHITE_BALANCE	(VOXL_CID_BASE+12)
#define VOXL_CID_DO_WHITE_BALANCE	(VOXL_CID_BASE+13)
#define VOXL_CID_RED_BALANCE		(VOXL_CID_BASE+14)
#define VOXL_CID_BLUE_BALANCE		(VOXL_CID_BASE+15)
#define VOXL_CID_GAMMA			(VOXL_CID_BASE+16)
#define VOXL_CID_WHITENESS		(VOXL_CID_GAMMA) /* Deprecated */
#define VOXL_CID_EXPOSURE		(VOXL_CID_BASE+17)
#define VOXL_CID_AUTOGAIN		(VOXL_CID_BASE+18)
#define VOXL_CID_GAIN			(VOXL_CID_BASE+19)
#define VOXL_CID_HFLIP			(VOXL_CID_BASE+20)
#define VOXL_CID_VFLIP			(VOXL_CID_BASE+21)




typedef enum {
  VOXL_PIX_FMT_RGB332,
  VOXL_PIX_FMT_ARGB555,
  VOXL_PIX_FMT_XRGB555,
  VOXL_PIX_FMT_ARGB555XW,
  VOXL_PIX_FMT_XRGB555XW,
  VOXL_PIX_FMT_RGB565,
  VOXL_PIX_FMT_RGB565X,
  VOXL_PIX_FMT_BGR666,
  VOXL_PIX_FMT_BGR24,
  VOXL_PIX_FMT_RGB24,
  VOXL_PIX_FMT_ABGR32,
  VOXL_PIX_FMT_XBGR32,
  VOXL_PIX_FMT_BGRA32,
  VOXL_PIX_FMT_BGRX32,
  VOXL_PIX_FMT_RGBA32,
  VOXL_PIX_FMT_RGBX32,
  VOXL_PIX_FMT_ARGB32,
  VOXL_PIX_FMT_XRGB32,

  VOXL_PIX_FMT_RGB444, 
  VOXL_PIX_FMT_RGB555, 
  VOXL_PIX_FMT_RGB555X,
  VOXL_PIX_FMT_BGR32, 
  VOXL_PIX_FMT_RGB32, 

  /* Grey formats */
  VOXL_PIX_FMT_GREY, 
  VOXL_PIX_FMT_Y4, 
  VOXL_PIX_FMT_Y6, 
  VOXL_PIX_FMT_Y10,
  VOXL_PIX_FMT_Y12,
  VOXL_PIX_FMT_Y16, 
  VOXL_PIX_FMT_Y16_BE, 
  VOXL_PIX_FMT_Y10BPACK, 

  /* Palette formats */
  VOXL_PIX_FMT_PAL8,

  /* Chrominance formats */
  VOXL_PIX_FMT_UV8,

  /* Luminance+Chrominance formats */
  VOXL_PIX_FMT_YVU410,
  VOXL_PIX_FMT_YVU420,
  VOXL_PIX_FMT_YVU420M,
  VOXL_PIX_FMT_YUYV,
  VOXL_PIX_FMT_YYUV,
  VOXL_PIX_FMT_YVYU,
  VOXL_PIX_FMT_UYVY,
  VOXL_PIX_FMT_VYUY,
  VOXL_PIX_FMT_YUV422P,
  VOXL_PIX_FMT_YUV411P,
  VOXL_PIX_FMT_Y41P,
  VOXL_PIX_FMT_YUV444,
  VOXL_PIX_FMT_YUV555,
  VOXL_PIX_FMT_YUV565,
  VOXL_PIX_FMT_YUV32,
  VOXL_PIX_FMT_YUV410,
  VOXL_PIX_FMT_YUV420,
  VOXL_PIX_FMT_YUV420M,
  VOXL_PIX_FMT_HI240,
  VOXL_PIX_FMT_HM12,
  VOXL_PIX_FMT_M420,

  /* two planes -- one Y, one Cr + Cb interleaved  */
  VOXL_PIX_FMT_NV12,
  VOXL_PIX_FMT_NV12M,
  VOXL_PIX_FMT_NV12MT,
  VOXL_PIX_FMT_NV12MT_16X16OXL_RAW,
  VOXL_PIX_FMT_NV21,
  VOXL_PIX_FMT_NV21M,
  VOXL_PIX_FMT_NV16,
  VOXL_PIX_FMT_NV16M,
  VOXL_PIX_FMT_NV61,
  VOXL_PIX_FMT_NV61M,
  VOXL_PIX_FMT_NV24,
  VOXL_PIX_FMT_NV42,

  /* Bayer formats - see http://www.siliconimaging.com/RGB%20Bayer.htm */
  VOXL_PIX_FMT_SBGGR8,
  VOXL_PIX_FMT_SGBRG8,
  VOXL_PIX_FMT_SGRBG8,
  VOXL_PIX_FMT_SRGGB8,

  /* compressed formats */
  VOXL_PIX_FMT_MJPEG,
  VOXL_PIX_FMT_JPEG,
  VOXL_PIX_FMT_PJPG,
  VOXL_PIX_FMT_DV,
  VOXL_PIX_FMT_MPEG,
  VOXL_PIX_FMT_FWHT,
  VOXL_PIX_FMT_H264,
  VOXL_PIX_FMT_H264_NO_SC,
  VOXL_PIX_FMT_H264_MVC,
  VOXL_PIX_FMT_HEVC,
  VOXL_PIX_FMT_H263,
  VOXL_PIX_FMT_MPEG1,
  VOXL_PIX_FMT_MPEG2,
  VOXL_PIX_FMT_MPEG4,
  VOXL_PIX_FMT_XVID,
  VOXL_PIX_FMT_VC1_ANNEX_G,
  VOXL_PIX_FMT_VC1_ANNEX_L,
  VOXL_PIX_FMT_VP8,
  VOXL_PIX_FMT_VP9,

  /*  Vendor-specific formats   */
  VOXL_PIX_FMT_WNVA,
  VOXL_PIX_FMT_SN9C10X,
  VOXL_PIX_FMT_PWC1,
  VOXL_PIX_FMT_PWC2
}VOXL_FORAMT;


typedef struct _GstVoxlError GstVoxlError;

#define GST_VOXL_IS_OPEN(o)      ((o)->video_fd > 0)
#define GST_V_IS_OPEN(o)      ((o)->video_fd > 0)
#define GST_VOXL_FPS_N(o)        (GST_VIDEO_INFO_FPS_N (&(o)->info))
#define GST_VOXL_FPS_D(o)        (GST_VIDEO_INFO_FPS_D (&(o)->info))
// #define GST_VOXL_BUFFER_POOL_CAST(obj) ((GstV4l2BufferPool*)(obj))

struct _GstVoxlError
{
    GError *error;
    gchar *dbg_message;
    const gchar *file;
    const gchar *func;
    gint line;
};

struct voxl_control 
{
	unsigned int		     id;
	unsigned int		     value;

};


#define GST_VOXL_ERROR_INIT { NULL, NULL }
#define GST_VOXL_ERROR(voxlerr,domain,code,msg,dbg) \
{\
  if (voxlerr) { \
    gchar *_msg = _gst_element_error_printf msg; \
    voxlerr->error = g_error_new_literal (GST_##domain##_ERROR, \
        GST_##domain##_ERROR_##code, _msg); \
    g_free (_msg); \
    voxlerr->dbg_message = _gst_element_error_printf dbg; \
    voxlerr->file = __FILE__; \
    voxlerr->func = GST_FUNCTION; \
    voxlerr->line = __LINE__; \
  } \
}




struct _GstVoxlSrc
{
  GstBaseSrc base_voxlsrc;

  GstPad *srcpad;

  context_data data;

  GAsyncQueue* m_queue;


  VoxlCameraDeviceChannel device_prop;

  guint device_channel;

  gchar device_name[VOXL_MAX_SIZE_PROP];

  GstBuffer* gst_buffer;
  //GstPushSrc pushsrc;

  GstBufferPool * pool;

};



struct _GstVoxlSrcClass
{
  GstBaseSrcClass base_voxlsrc_class;

};

GType gst_voxlsrc_get_type (void);

G_END_DECLS

#endif



