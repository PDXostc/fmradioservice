 /* GStreamer
 * Copyright (C) 2014, Collabora Ltd. <info@collabora.com>
 *
 * gstsdrjfmsrc.c:
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

/* Note, the plugin code is left LPGLv2, so this code can be copy pasted to
 * form other FM sources later if it makes sense. But the plugin will remain
 * GPLv2 since the backend is from GPLv2 program.
 */

/**
 * SECTION:element-sdrjfmsrc
 *
 * This element lets capture from RTL SDR supported radio card.
 *
 * <refsect2>
 * <title>Example pipelines</title>
 * |[
 * gst-launch-1.0 -v sdrjfmsrc frequency=97700000 ! pulsesink
 * ]| will playback live FM radio channel 97.7.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstsdrjfmsrc.h"
#include "fm_radio_common.h"

GST_DEBUG_CATEGORY_EXTERN (sdrjfm_debug);
#define GST_CAT_DEFAULT sdrjfm_debug


#define DEFAULT_FREQUENCY_STEP     100000
#define DEFAULT_INTERVAL              100
#define DEFAULT_THRESHOLD              30

const char DEFAULT_STATION_LABEL[9] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
const char DEFAULT_RADIO_TEXT[65] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
				     '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
				     '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
				     '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
				     '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
				     '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
				     '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
				     '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
				     '\0' };

enum
{
  PROP_0,
  PROP_MIN_FREQUENCY,
  PROP_MAX_FREQUENCY,
  PROP_FREQUENCY_STEP,
  PROP_FREQUENCY,
  PROP_INTERVAL,
  PROP_THRESHOLD,
  PROP_STATION_LABEL,
  PROP_RADIO_TEXT
};

/* signals and args */
enum
{
  SIGNAL_SEEK_UP,
  SIGNAL_SEEK_DOWN,
  SIGNAL_CANCEL_SEEK,
  LAST_SIGNAL
};


#define gst_sdrjfm_src_parent_class parent_class

extern "C" {
G_DEFINE_TYPE (GstSdrjfmSrc, gst_sdrjfm_src, GST_TYPE_AUDIO_SRC);
}

static GstStaticPadTemplate sdrjfmsrc_src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE(F32) ", "
        "layout = (string) interleaved, "
        "rate = (int) 44100, "
        "channels = (int) 2; ")
    );

static guint signals[LAST_SIGNAL] = { 0 };

static void
 gst_sdrjfm_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSdrjfmSrc *self = GST_SDRJFM_SRC (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_MIN_FREQUENCY:
      self->min_freq = g_value_get_int (value);
      break;
    case PROP_MAX_FREQUENCY:
      self->max_freq = g_value_get_int (value);
      break;
    case PROP_FREQUENCY_STEP:
      self->freq_step = g_value_get_int (value);
      break;
    case PROP_FREQUENCY:
      {
        gint frequency = g_value_get_int (value);
	self->frequency = frequency;
	GST_OBJECT_UNLOCK (self);
	if (self->radio)
	  self->radio->setTuner( frequency );
      }
      break;
    case PROP_INTERVAL:
      self->interval = g_value_get_int (value);
      break;
    case PROP_THRESHOLD:
      self->threshold = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  if (prop_id != PROP_FREQUENCY)
    GST_OBJECT_UNLOCK (self);
}

static void
gst_sdrjfm_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSdrjfmSrc *self = GST_SDRJFM_SRC (object);

  GST_OBJECT_LOCK (self);

  switch (prop_id) {
    case PROP_MIN_FREQUENCY:
      g_value_set_int (value, self->min_freq);
      break;
    case PROP_MAX_FREQUENCY:
      g_value_set_int (value, self->max_freq);
      break;
    case PROP_FREQUENCY_STEP:
      g_value_set_int (value, self->freq_step);
      break;
    case PROP_FREQUENCY:
      g_value_set_int (value, self->frequency);
      break;
    case PROP_INTERVAL:
      g_value_set_int (value, self->interval);
      break;
    case PROP_THRESHOLD:
      g_value_set_int (value, self->threshold);
      break;
    case PROP_STATION_LABEL:
      g_value_set_string (value, self->station_label);
      break;
    case PROP_RADIO_TEXT:
      g_value_set_string (value, self->radio_text);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (self);
}

typedef struct _BusMessageData
{
  GstSdrjfmSrc *src;
  GstMessage *msg;
} BusMessageData;

static gboolean
gst_sdrjfm_src_send_bus_message_idle (void *user_data)
{
  BusMessageData *data = static_cast<BusMessageData *>(user_data);
  gst_element_post_message (GST_ELEMENT(data->src), data->msg);
  delete data;
  return FALSE;
}

static void
gst_sdrjfm_src_send_bus_message(GstSdrjfmSrc *self, GstStructure *s)
{

  BusMessageData *data;

  data = new BusMessageData;
  data->src = self;
  data->msg = gst_message_new_element (GST_OBJECT (self), s);

  g_idle_add (gst_sdrjfm_src_send_bus_message_idle, data);
}

template <typename V>
static void
gst_sdrjfm_src_send_bus_message(GstSdrjfmSrc *self, const char *name,
				const char*field_name, GType type, V field_value)
{
  GstStructure *s;

  s = gst_structure_new (name, field_name, type, field_value, NULL);

  gst_sdrjfm_src_send_bus_message (self, s);
}

static void
gst_sdrjfm_src_send_bus_message_empty(GstSdrjfmSrc * self, const char *name)
{
  GstStructure *s;

  s = gst_structure_new_empty (name);

  gst_sdrjfm_src_send_bus_message (self, s);
}

static void
gst_sdrjfm_src_frequency_changed (void *user_data, int32_t frequency)
{
  GstSdrjfmSrc *self = static_cast<GstSdrjfmSrc *>(user_data);
  gst_sdrjfm_src_send_bus_message (self, "sdrjfmsrc-frequency-changed",
				   "frequency", G_TYPE_INT, frequency);
}

static void
gst_sdrjfm_src_send_bus_message_string(GstSdrjfmSrc * self, const char *name,
				       const gchar * field_name, const gchar * field_value)
{
  gst_sdrjfm_src_send_bus_message (self, name,
				   field_name, G_TYPE_STRING, field_value);
}

static void
gst_sdrjfm_src_send_bus_message_rds_string(GstSdrjfmSrc * self, const char *event,
					   const gchar * field_name, const gchar * field_value)
{
  GString * message_name = g_string_new(NULL);
  g_string_printf(message_name, "sdrjfmsrc-rds-%s-%s", field_name, event);

  gst_sdrjfm_src_send_bus_message (self, message_name->str,
				   field_name, G_TYPE_STRING, field_value);

  g_string_free (message_name, TRUE);
}

static void
gst_sdrjfm_src_send_bus_message_rds_empty(GstSdrjfmSrc * self, const char *event,
					  const char *field_name)
{
  GString * name = g_string_new(NULL);
  g_string_printf(name, "sdrjfmsrc-rds-%s-%s", field_name, event);

  gst_sdrjfm_src_send_bus_message_empty (self, name->str);

  g_string_free (name, TRUE);
}

static void
gst_sdrjfm_src_rds_clear(GstSdrjfmSrc * self, char *field, size_t field_len,
			 const char * default_value,
			 const char * field_name)
{
  GST_OBJECT_LOCK (self);

  strncpy (field, default_value, field_len);

  gst_sdrjfm_src_send_bus_message_rds_empty(self, "clear", field_name);

  GST_OBJECT_UNLOCK (self);
}

static void
gst_sdrjfm_src_rds_station_label_clear(void * user_data)
{
  GstSdrjfmSrc *self = static_cast<GstSdrjfmSrc *>(user_data);
  gst_sdrjfm_src_rds_clear(self, self->station_label, sizeof (self->station_label),
			   DEFAULT_STATION_LABEL, "station-label");
}

static void
gst_sdrjfm_src_rds_radio_text_clear(void * user_data)
{
  GstSdrjfmSrc *self = static_cast<GstSdrjfmSrc *>(user_data);
  gst_sdrjfm_src_rds_clear(self, self->radio_text, sizeof (self->radio_text),
			   DEFAULT_RADIO_TEXT, "radio-text");
}

static void
gst_sdrjfm_src_rds_set(const char *string,
		       char *dest, size_t dest_len)
{
  // Reverse through the provided label, setting NULs locally until
  // non-space is seen and then just copy
  int i;
  for (i = dest_len - 2; i >= 0; --i)
    {
      if (g_ascii_isspace (string[i]))
        dest[i] = '\0';
      else
	break;
    }
  if (i >= 0)
      strncpy (dest, string, i + 1);
}

static void
gst_sdrjfm_src_rds_change(GstSdrjfmSrc * self, const char *string, char *dest,
			  size_t dest_len, const char *field_name)
{
  GST_OBJECT_LOCK (self);

  gst_sdrjfm_src_rds_set(string, dest, dest_len);

  gst_sdrjfm_src_send_bus_message_rds_string(self, "change", field_name, dest);

  GST_OBJECT_UNLOCK (self);
}

static void
gst_sdrjfm_src_rds_station_label_change(const char *label, void *user_data)
{
  GstSdrjfmSrc *self = static_cast<GstSdrjfmSrc *>(user_data);
  gst_sdrjfm_src_rds_change(self, label, &self->station_label[0], sizeof (self->station_label),
			    "station-label");
}
static void
gst_sdrjfm_src_rds_radio_text_change(const char *text, void *user_data)
{
  GstSdrjfmSrc *self = static_cast<GstSdrjfmSrc *>(user_data);
  gst_sdrjfm_src_rds_change(self, text, &self->radio_text[0], sizeof (self->radio_text),
			    "radio-text");
}

static void
gst_sdrjfm_src_rds_complete(GstSdrjfmSrc *self, const char *string, const char *field_name,
			    char *field, size_t field_len)
{
  GST_OBJECT_LOCK (self);

  gst_sdrjfm_src_rds_set(string, field, field_len);

  gst_sdrjfm_src_send_bus_message_rds_string(self, "complete", field_name, field);

  GST_OBJECT_UNLOCK (self);
}

static void
gst_sdrjfm_src_rds_station_label_complete(const char *label, void *user_data)
{
  GstSdrjfmSrc *self = static_cast<GstSdrjfmSrc *>(user_data);

  gst_sdrjfm_src_rds_complete(self, label, "station-label",
			      self->station_label, sizeof (self->station_label));
}

static void
gst_sdrjfm_src_rds_radio_text_complete(const char *label, void *user_data)
{
  GstSdrjfmSrc *self = static_cast<GstSdrjfmSrc *>(user_data);

  gst_sdrjfm_src_rds_complete(self, label, "radio-text",
			      self->radio_text, sizeof (self->radio_text));
}

static gboolean
gst_sdrjfm_src_open (GstAudioSrc * asrc)
{
  GstSdrjfmSrc *self = GST_SDRJFM_SRC (asrc);

  self->radio = new RadioInterface(self->frequency,
				   gst_sdrjfm_src_rds_station_label_clear,
				   gst_sdrjfm_src_rds_station_label_change,
				   gst_sdrjfm_src_rds_station_label_complete,
				   gst_sdrjfm_src_rds_radio_text_clear,
				   gst_sdrjfm_src_rds_radio_text_change,
				   gst_sdrjfm_src_rds_radio_text_complete,
				   self);

  GST_INFO_OBJECT (self, "Created new SDR-J FM Radio object with frequency %u",
		   self->frequency);

  self->radio->setFrequencyChangeCB (gst_sdrjfm_src_frequency_changed, self);

  return TRUE;
}

static gboolean
gst_sdrjfm_src_close (GstAudioSrc * asrc)
{
  GstSdrjfmSrc *self = GST_SDRJFM_SRC (asrc);

  GST_DEBUG_OBJECT (asrc, "Closing FM source.");
  if (self->radio)
    {
      self->radio->stop();
      GST_DEBUG_OBJECT(self, "Deleting radio...");
      delete self->radio;
      GST_DEBUG_OBJECT(self, "...radio deleted");
      self->radio = 0;
    }

  return TRUE;
}

static gboolean
gst_sdrjfm_src_prepare (GstAudioSrc * asrc, GstAudioRingBufferSpec * spec)
{
  GstSdrjfmSrc *self = GST_SDRJFM_SRC (asrc);

  GST_DEBUG_OBJECT(self, "Starting radio...");
  self->radio->start();
  GST_DEBUG_OBJECT(self, "...radio started");

  return TRUE;
}

static gboolean
gst_sdrjfm_src_unprepare (GstAudioSrc * asrc)
{
  GstSdrjfmSrc *self = GST_SDRJFM_SRC (asrc);

  GST_DEBUG_OBJECT(self, "Stopping radio..");
  self->radio->stop();
  GST_DEBUG_OBJECT(self, "...radio stopped");

  return TRUE;
}

static guint
gst_sdrjfm_src_read (GstAudioSrc * asrc, gpointer data, guint length,
    GstClockTime * timestamp)
{
  const guint samplesRequired = length / sizeof(DSPFLOAT);

  GstSdrjfmSrc *self =  GST_SDRJFM_SRC (asrc);
  DSPFLOAT *samples = static_cast<DSPFLOAT *>(data);
  guint remaining = samplesRequired;

  while (remaining) {
    gint count = self->radio->getSamples(samples, remaining);

    if (count == -1)
      {
	 // Either we're stopped, or the read has been cancelled
	 GST_TRACE_OBJECT(self, "Read cancelled or stopped");
	 //return length - (remaining * sizeof(DSPFLOAT));

	 // Work around bug in GstAudioSrc
	 return G_MAXUINT;
      }

    GST_TRACE_OBJECT(self, "Read %i samples out of %u remaining from %u (length: %u)",
		     count, remaining, samplesRequired, length);

    samples += count;
    remaining -= count;
  }

  return length;
}

static guint
gst_sdrjfm_src_delay (GstAudioSrc * asrc)
{
  GstSdrjfmSrc *self =  GST_SDRJFM_SRC (asrc);
  return self->radio->getWaitingSamples ();
  //GST_FIXME_OBJECT (asrc, "not implemented");
  //return 0;
}

static void
gst_sdrjfm_src_reset (GstAudioSrc * asrc)
{
  GstSdrjfmSrc *self =  GST_SDRJFM_SRC (asrc);
  self->radio->cancelGet ();
}

static void
gst_sdrjfm_src_dispose (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_sdrjfm_src_finalize (GstSdrjfmSrc * self)
{
  G_OBJECT_CLASS (parent_class)->finalize (G_OBJECT (self));
}

static void
gst_sdrjfm_src_station_found (int32_t frequency, void *user_data)
{
  GstSdrjfmSrc *self = static_cast<GstSdrjfmSrc *>(user_data);
  GST_DEBUG_OBJECT (self, "Station found at frequency %i", frequency);

  self->frequency = frequency;
  gst_sdrjfm_src_send_bus_message (self, "sdrjfmsrc-station-found",
				   "frequency", G_TYPE_INT, frequency);
}

static void
gst_sdrjfm_src_do_seek(GstSdrjfmSrc * self, int32_t step)
{
  self->radio->seek(self->threshold, self->min_freq, self->max_freq, step,
    self->interval, gst_sdrjfm_src_station_found, self);
}

static void
gst_sdrjfm_src_seek_up (GstSdrjfmSrc * self)
{
  return gst_sdrjfm_src_do_seek(self, self->freq_step);
}

static void
gst_sdrjfm_src_seek_down (GstSdrjfmSrc * self)
{
  return gst_sdrjfm_src_do_seek(self, -self->freq_step);
}

static void
gst_sdrjfm_src_cancel_seek (GstSdrjfmSrc * self)
{
  self->radio->cancelSeek();
}

static void
gst_sdrjfm_src_init (GstSdrjfmSrc * self)
{
  GstAudioBaseSrc *basrc = GST_AUDIO_BASE_SRC (self);

  self->min_freq = FM_RADIO_SERVICE_MIN_FREQ;
  self->max_freq = FM_RADIO_SERVICE_MAX_FREQ;
  self->freq_step = DEFAULT_FREQUENCY_STEP;
  self->frequency = FM_RADIO_SERVICE_DEF_FREQ;
  self->interval = DEFAULT_INTERVAL;
  self->threshold = DEFAULT_THRESHOLD;

  strncpy(self->station_label, DEFAULT_STATION_LABEL, sizeof (self->station_label));
  strncpy(self->radio_text, DEFAULT_RADIO_TEXT, sizeof (self->radio_text));

  basrc->buffer_time = 5000000;

  gst_audio_base_src_set_provide_clock (basrc, FALSE);
}

static void
gst_sdrjfm_src_class_init (GstSdrjfmSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstAudioSrcClass *gstaudiosrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstaudiosrc_class = (GstAudioSrcClass *) klass;

  gobject_class->dispose = gst_sdrjfm_src_dispose;
  gobject_class->finalize = (GObjectFinalizeFunc) gst_sdrjfm_src_finalize;
  gobject_class->get_property = gst_sdrjfm_src_get_property;
  gobject_class->set_property = gst_sdrjfm_src_set_property;

  gstaudiosrc_class->open = GST_DEBUG_FUNCPTR (gst_sdrjfm_src_open);
  gstaudiosrc_class->prepare = GST_DEBUG_FUNCPTR (gst_sdrjfm_src_prepare);
  gstaudiosrc_class->unprepare = GST_DEBUG_FUNCPTR (gst_sdrjfm_src_unprepare);
  gstaudiosrc_class->close = GST_DEBUG_FUNCPTR (gst_sdrjfm_src_close);
  gstaudiosrc_class->read = GST_DEBUG_FUNCPTR (gst_sdrjfm_src_read);
  gstaudiosrc_class->delay = GST_DEBUG_FUNCPTR (gst_sdrjfm_src_delay);
  gstaudiosrc_class->reset = GST_DEBUG_FUNCPTR (gst_sdrjfm_src_reset);

  klass->seek_up = GST_DEBUG_FUNCPTR (gst_sdrjfm_src_seek_up);
  klass->seek_down = GST_DEBUG_FUNCPTR (gst_sdrjfm_src_seek_down);
  klass->cancel_seek = GST_DEBUG_FUNCPTR (gst_sdrjfm_src_cancel_seek);

  g_object_class_install_property (gobject_class, PROP_MIN_FREQUENCY,
      g_param_spec_int ("min-frequency", "Minimum Frequency",
          "Minimum frequency used in seeks (in Hz)", 0, G_MAXINT, FM_RADIO_SERVICE_MIN_FREQ,
			static_cast<GParamFlags>( G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_MAX_FREQUENCY,
      g_param_spec_int ("max-frequency", "Maximum Frequency",
          "Maximum frequency used in seeks (in Hz)", 0, G_MAXINT, FM_RADIO_SERVICE_MAX_FREQ,
			static_cast<GParamFlags>( G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FREQUENCY_STEP,
      g_param_spec_int ("frequency-step", "Frequency Step",
          "Frequency steps used in seeks (in Hz)", 1, G_MAXINT, DEFAULT_FREQUENCY_STEP,
			static_cast<GParamFlags>( G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_FREQUENCY,
      g_param_spec_int ("frequency", "Frequency",
          "Frequency to receive", 0, G_MAXINT, FM_RADIO_SERVICE_DEF_FREQ,
    static_cast<GParamFlags>(G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_INTERVAL,
      g_param_spec_int ("interval", "Interval",
			"Interval between frequency hops during seeks",
			0, G_MAXINT, DEFAULT_INTERVAL,
			static_cast<GParamFlags>(G_PARAM_READWRITE
						 | GST_PARAM_MUTABLE_PLAYING
						 | G_PARAM_STATIC_STRINGS)));
 
  g_object_class_install_property (gobject_class, PROP_THRESHOLD,
      g_param_spec_int ("threshold", "Threshold",
			"Signal-to-noise threshold to consider a station present during seeks",
			0, G_MAXINT, DEFAULT_THRESHOLD,
			static_cast<GParamFlags>(G_PARAM_READWRITE
						 | GST_PARAM_MUTABLE_PLAYING
						 | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_STATION_LABEL,
      g_param_spec_string ("station-label", "Station Label",
			"RDS label for the received station",
			DEFAULT_STATION_LABEL,
			static_cast<GParamFlags>(G_PARAM_READABLE
						 | GST_PARAM_MUTABLE_PLAYING
						 | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_RADIO_TEXT,
      g_param_spec_string ("radio-text", "Radio Text",
			"RDS text for the received station",
			DEFAULT_RADIO_TEXT,
			static_cast<GParamFlags>(G_PARAM_READABLE
						 | GST_PARAM_MUTABLE_PLAYING
						 | G_PARAM_STATIC_STRINGS)));

  signals[SIGNAL_SEEK_UP] =
      g_signal_new ("seek-up", G_TYPE_FROM_CLASS (klass),
		    static_cast<GSignalFlags>( G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		    G_STRUCT_OFFSET (GstSdrjfmSrcClass, seek_up), NULL, NULL,
		    g_cclosure_marshal_generic, G_TYPE_NONE, 0, G_TYPE_NONE);

  signals[SIGNAL_SEEK_DOWN] =
      g_signal_new ("seek-down", G_TYPE_FROM_CLASS (klass),
		    static_cast<GSignalFlags>( G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION ),
		    G_STRUCT_OFFSET (GstSdrjfmSrcClass, seek_down), NULL, NULL,
		    g_cclosure_marshal_generic, G_TYPE_NONE, 0, G_TYPE_NONE);

  signals[SIGNAL_CANCEL_SEEK] =
      g_signal_new ("cancel-seek", G_TYPE_FROM_CLASS (klass),
		    static_cast<GSignalFlags>( G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION ),
		    G_STRUCT_OFFSET (GstSdrjfmSrcClass, cancel_seek), NULL, NULL,
		    g_cclosure_marshal_generic, G_TYPE_NONE, 0, G_TYPE_NONE);

  gst_element_class_set_static_metadata (gstelement_class, "FM Radio Source (SDR-J)",
      "Source/Audio",
      "Capture and demodulate FM radio",
      "Collabora <info@collabora.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sdrjfmsrc_src_factory));
}

