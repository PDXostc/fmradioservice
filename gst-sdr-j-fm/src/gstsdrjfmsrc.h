/* GStreamer
 * Copyright (C) Collabora Ltd. <info@collabora.com>
 *
 * gstsdrjfmsrc.h:
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

#ifndef __GST_SDRJFM_SRC_H__
#define __GST_SDRJFM_SRC_H__


#include <gst/gst.h>
#include <gst/audio/gstaudiosrc.h>

#include <gui.h>

//typedef struct _GstSdrjfmSrc GstSdrjfmSrc;
typedef struct _GstSdrjfmSrcClass GstSdrjfmSrcClass;

/** \brief The SDR-J FM source element.
 *
 * This element will send a variety of bus messages.
 * <DL>
 * 
 * <DT>`sdrjfmsrc-frequency-changed`</DT>
 * <DD>Emitted when the radio receiver's frequency is
 * changed, for any reason.
 * <P><B>Properties</B>
 * <TABLE>
 * <TR>
 * <TD>Name</TD><TD>Type</TD><TD>Description</TD><TD>Example</TD>
 * </TR>
 * <TR><TD>`frequency`</TD><TD>`G_TYPE_INT`</TD><TD>Receiver
 * frequency, in Hz</TD><TD>97600000</TD></TR>
 * </TABLE>
 * </DD>
 * 
 * <DT>`sdrjfmsrc-station-found`</DT>
 * <DD>Emitted when a seek operation finds a station.
 * <P><B>Properties</B>
 * <TABLE>
 * <TR>
 * <TD>Name</TD><TD>Type</TD><TD>Description</TD><TD>Example</TD>
 * </TR>
 * <TR><TD>`frequency`</TD><TD>`G_TYPE_INT`</TD><TD>The located
 * station's frequency, in Hz</TD><TD>97600000</TD></TR>
 * </TABLE>
 * </DD>
 * 
 * <DT>`sdrjfmsrc-rds-station-label-clear`</DT>
 * <DD>Emitted when the RDS station label has been cleared.  For
 * example, when changing frequency or because the tuned station has
 * changed the transmitted label.
 * <P><B>Properties</B>
 * <P>This message has no properties.
 * </DD>
 * 
 * <DT>`sdrjfmsrc-rds-station-label-change`</DT>
 * <DD>Emitted when the RDS station label changes content.  As different
 * segments of the label are received, this message will be emitted
 * each time a new segment is successfully decoded.  Empty segments
 * are filled with spaces.
 * <P><B>Properties</B>
 * <TABLE>
 * <TR>
 * <TD>Name</TD><TD>Type</TD><TD>Description</TD><TD>Example</TD>
 * </TR>
 * <TR><TD>`station-label`</TD><TD>`G_TYPE_STRING`</TD><TD>The RDS
 * station label, or as much of it as has been received</TD>
 * <TD>`Ci&nbsp;&nbsp;Ta&nbsp;&nbsp;`, `Ci&nbsp;&nbsp;Talk`, `CityTalk`</TD></TR>
 * </TABLE>
 * </DD>
 * 
 * <DT>`sdrjfmsrc-rds-station-label-complete`</DT>
 * <DD>Emitted when each segment in the RDS station label has been
 * successfully decoded.  This message will be emitted in addition to
 * a `sdrjfmsrc-rds-station-label-change` message.
 * <P><B>Properties</B>
 * <TABLE>
 * <TR>
 * <TD>Name</TD><TD>Type</TD><TD>Description</TD><TD>Example</TD>
 * <TR><TD>`station-label`</TD><TD>`G_TYPE_STRING`</TD><TD>The
 * complete RDS station label</TD>
 * <TD>`CityTalk`</TD></TR>
 * </TR>
 * </TABLE>
 * </DD>
 * 
 * <DT>`sdrjfmsrc-rds-radio-text-clear`</DT>
 * <DD>Emitted when the RDS radio text has been cleared.  For
 * example, when changing frequency or because the tuned station has
 * changed the transmitted label.
 * <P><B>Properties</B>
 * <P>This message has no properties.
 * </DD>
 * 
 * <DT>`sdrjfmsrc-rds-radio-text-change`</DT>
 * <DD>Emitted when the RDS radio text changes content.  As different
 * segments of the text are received, this message will be emitted
 * each time a new segment is successfully decoded.  Empty segments
 * are filled with spaces.
 * <P><B>Properties</B>
 * <TABLE>
 * <TR>
 * <TD>Name</TD><TD>Type</TD><TD>Description</TD><TD>Example</TD>
 * </TR>
 * <TR><TD>`radio-text`</TD><TD>`G_TYPE_STRING`</TD><TD>The RDS
 * radio text, or as much of it as has been received</TD>
 * <TD>`This&nbsp;is&nbsp;City&nbsp;Tal&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Liverpool`</TD></TR>
 * </TABLE>
 * </DD>
 * 
 * <DT>`sdrjfmsrc-rds-radio-text-complete`</DT>
 * <DD>Emitted when each segment in the RDS radio text has been
 * successfully decoded.  This message will be emitted in addition to
 * a `sdrjfmsrc-rds-radio-text-change` message.
 * <P><B>Properties</B>
 * <TABLE>
 * <TR>
 * <TD>Name</TD><TD>Type</TD><TD>Description</TD><TD>Example</TD>
 * <TR><TD>`radio-text`</TD><TD>`G_TYPE_STRING`</TD><TD>The complete RDS
 * radio text</TD>
 * <TD>`This&nbsp;is&nbsp;City&nbsp;Talk&nbsp;105.9&nbsp;Liverpool`</TD></TR>
 * </TR>
 * </TABLE>
 * </DD>
 * 
 * </DL>
 */
struct GstSdrjfmSrc {
  GstAudioSrc    src;

  /** The receiver frequency, in Hz */
  gint frequency;
  /** Lower bound frequency for seeking, in Hz */
  gint min_freq;
  /** Upper bound frequency for seeking, in Hz */
  gint max_freq;
  /** \brief The amount, in Hz, by which the frequency is increased
   * in each iteration during a seek.
   * 
   * The sign of this variable indicates the direction of the seek.
   */
  gint freq_step;
  /** The time, in milliseconds, allowed for sampling a frequency
   * during seeking.
   */
  gint interval;
  /** The signal-to-noise ratio beyond which a station is considered
   * found during seeking, in dB
   */
  gint threshold;
  /** \brief The RDS station label.
   * 
   * This is an eight-character buffer.  We store
   * it with a closing NUL character and also insert a NUL when there is only
   * trailing whitespace.
   */
  gchar station_label[9];
  /** \brief The RDS radio text.
   * 
   * This is a sixty-four-character buffer.  We store
   * it with a closing NUL character and also insert a NUL when there is only
   * trailing whitespace.
   */
  gchar radio_text[65];

  RadioInterface *radio;
};

struct _GstSdrjfmSrcClass {
  GstAudioSrcClass parent_class;

  /* action signals */
  void        (*seek_up)         (GstSdrjfmSrc *src);
  void        (*seek_down)       (GstSdrjfmSrc *src);
  void        (*cancel_seek)     (GstSdrjfmSrc *src);
};

extern "C" {

#define GST_TYPE_SDRJFM_SRC           (gst_sdrjfm_src_get_type())
#define GST_SDRJFM_SRC(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SDRJFM_SRC,GstSdrjfmSrc))
#define GST_SDRJFM_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SDRJFM_SRC,GstSdrjfmSrcClass))
#define GST_IS_SDRJFM_SRC(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SDRJFM_SRC))
#define GST_IS_SDRJFM_SRC_CLASS(klas) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SDRJFM_SRC))

GType gst_sdrjfm_src_get_type(void);

}

#endif /* __GST_SDRJFM_SRC_H__ */
