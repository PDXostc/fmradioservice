#include <glib.h>
#include <gst/gst.h>
#include <string.h>

GST_DEBUG_CATEGORY (sdrjfm_debug);
#define GST_CAT_DEFAULT sdrjfm_debug

#define MIN_FREQ              88000000
#define MAX_FREQ             108000000
#define RDS_CAPABLE_STATION  105900000

typedef enum
{
  SEEK_START = 1,
  SEEK_WAIT,
  SEEK_UP,
  SEEK_DOWN,
  SEEK_DONE
} SeekState;

typedef enum
{
  RDS_STATION_LABEL = 1,
  RDS_RADIO_TEXT
} RDSStringType;

typedef enum
{
  RDS_FIRST = 1,
  RDS_CLEAR,
  RDS_SECOND
} RDSState;

typedef enum
{
  STARTSTOP_BEGIN = 1,
  STARTSTOP_STOP,
  STARTSTOP_SECOND_START
} StartStopState;

typedef struct _TestData TestData;
struct _TestData
{
  GstElement *pipeline;
  GstElement *fmsrc;
  void (*playing_cb) (TestData*);
  void (*frequency_changed_cb) (TestData*, gint);
  void (*station_found_cb) (TestData*, gint);
  void (*rds_clear_cb) (TestData*, RDSStringType);
  void (*rds_change_cb) (TestData*, RDSStringType, const gchar *);
  void (*rds_complete_cb) (TestData*, RDSStringType, const gchar *);
  gint timeout;
  GMainLoop *loop;
  gint freqs[10];
  gint idx;
  gint target_freq;
  guint timeout_id;
  SeekState seek_state;
  RDSStringType rds_type;
  RDSState rds_state;
  StartStopState startstop_state;
};

static gboolean
bus_cb (GstBus *bus, GstMessage *message, gpointer user_data)
{
  TestData *data = user_data;
  GError *error = NULL;

  switch (message->type) {
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_MESSAGE_SRC (message) == GST_OBJECT (data->pipeline)) {
        GstState state;
        gst_message_parse_state_changed (message, NULL, &state, NULL);
        if (state == GST_STATE_PLAYING && data->playing_cb)
          data->playing_cb (data);
      }
      break;

    case GST_MESSAGE_ELEMENT:
      if (GST_MESSAGE_SRC (message) == GST_OBJECT (data->fmsrc)) {
        const GstStructure *s = gst_message_get_structure (message);

	if (gst_structure_has_field_typed (s, "frequency", G_TYPE_INT))
	  {
	    gint freq;
	    gst_structure_get_int (s, "frequency", &freq);
	    g_assert_cmpint (freq, >=, MIN_FREQ);
	    g_assert_cmpint (freq, <=, MAX_FREQ);


	    if (gst_structure_has_name (s, "sdrjfmsrc-frequency-changed")) {
	      if (data->frequency_changed_cb)
		data->frequency_changed_cb (data, freq);
	    } else if (gst_structure_has_name (s, "sdrjfmsrc-station-found")) {
	      if (data->station_found_cb)
		data->station_found_cb (data, freq);
	    }
	  }
	else if (gst_structure_has_name (s, "sdrjfmsrc-rds-station-label-clear"))
	  {
		if (data->rds_clear_cb)
		  data->rds_clear_cb (data, RDS_STATION_LABEL);
	  }
	else if (gst_structure_has_name (s, "sdrjfmsrc-rds-radio-text-clear"))
	  {
		if (data->rds_clear_cb)
		  data->rds_clear_cb (data, RDS_RADIO_TEXT);
	  }
	else if (gst_structure_has_field_typed (s, "station-label", G_TYPE_STRING))
	  {

	    const gchar *label = gst_structure_get_string (s, "station-label");
	    if (gst_structure_has_name (s, "sdrjfmsrc-rds-station-label-change"))
	      {
		if (data->rds_change_cb)
		  data->rds_change_cb (data, RDS_STATION_LABEL, label);
	      }
	    else if (gst_structure_has_name (s, "sdrjfmsrc-rds-station-label-complete"))
	      {
		if (data->rds_complete_cb)
		  data->rds_complete_cb (data, RDS_STATION_LABEL, label);
	      }
	  }
	else if (gst_structure_has_field_typed (s, "radio-text", G_TYPE_STRING))
	  {

	    const gchar *label = gst_structure_get_string (s, "radio-text");
	    if (gst_structure_has_name (s, "sdrjfmsrc-rds-radio-text-change"))
	      {
		if (data->rds_change_cb)
		  data->rds_change_cb (data, RDS_RADIO_TEXT, label);
	      }
	    else if (gst_structure_has_name (s, "sdrjfmsrc-rds-radio-text-complete"))
	      {
		if (data->rds_complete_cb)
		  data->rds_complete_cb (data, RDS_RADIO_TEXT, label);
	      }
	  }
      }
      break;

    case GST_MESSAGE_ERROR:
      gst_message_parse_error (message, &error, NULL);
      g_assert_no_error (error);
      break;

    case GST_MESSAGE_WARNING:
      gst_message_parse_warning (message, &error, NULL);
      GST_WARNING ("Warning: `%s'", error->message);
      if (strcmp (error->message, "Can't record audio fast enough") == 0)
	break;
      g_assert_no_error (error);
      break;

    default:
      break;
  }

  return TRUE;
}

static TestData *
tearup (gint freq, void (*playing_cb) (TestData*),
	void (*frequency_changed_cb) (TestData*, gint),
	void (*station_found_cb) (TestData*, gint),
	void (*rds_clear_cb) (TestData*, RDSStringType),
	void (*rds_change_cb) (TestData*, RDSStringType, const gchar *),
	void (*rds_complete_cb) (TestData*, RDSStringType, const gchar *))
{
  GError *error = NULL;
  TestData *data = g_slice_new0 (TestData);
  GstBus *bus;

  data->pipeline =
      gst_parse_launch ("sdrjfmsrc name=fmsrc ! queue ! pulsesink",
      &error);
  g_assert_no_error (error);
  g_assert(data->pipeline != NULL);

  data->fmsrc = gst_bin_get_by_name (GST_BIN (data->pipeline), "fmsrc");
  g_assert(data->fmsrc != NULL);

  g_object_set (data->fmsrc,
      "frequency", freq,
      NULL);

  data->timeout = 60;
  data->seek_state = SEEK_START;
  data->rds_state = RDS_FIRST;
  data->startstop_state = STARTSTOP_BEGIN;
  data->playing_cb = playing_cb;
  data->frequency_changed_cb = frequency_changed_cb;
  data->station_found_cb = station_found_cb;
  data->rds_clear_cb = rds_clear_cb;
  data->rds_change_cb = rds_change_cb;
  data->rds_complete_cb = rds_complete_cb;

  bus = gst_pipeline_get_bus (GST_PIPELINE (data->pipeline));
  gst_bus_add_watch (bus, bus_cb, data);
  g_object_unref (bus);

  data->loop = g_main_loop_new (NULL, FALSE);

  gst_element_set_state (data->pipeline, GST_STATE_PLAYING);

  return data;
}

static void
teardown (TestData *data)
{
  gst_element_set_state (data->pipeline, GST_STATE_NULL);
  g_object_unref (data->fmsrc);
  g_object_unref (data->pipeline);
  g_main_loop_unref (data->loop);
  g_slice_free (TestData, data);
}

static gboolean
test_timed_out_cb (gpointer user_data)
{
  g_assert ("Test timed out" == 0);
  return FALSE;
}

static void
test_run (TestData *data)
{
  guint timeout;

  timeout = g_timeout_add_seconds (data->timeout, test_timed_out_cb, data);

  g_main_loop_run (data->loop);
  g_source_remove (timeout);

  teardown (data);
}

static void
test_done (TestData *data)
{
  g_main_loop_quit (data->loop);
}

static void
test_tune_playing_cb (TestData *data)
{
  data->freqs[0] =  96700000;
  data->freqs[1] =  93300000;
  data->freqs[2] = 105900000;
  data->freqs[3] =  88900000;
  data->freqs[4] = 0;

  data->idx = 0;
  data->target_freq = data->freqs[data->idx];
  GST_DEBUG_OBJECT (data->fmsrc, "Setting first frequency to %i", data->target_freq);
  g_object_set (data->fmsrc, "frequency", data->target_freq, NULL);
}

static void
test_tune_freq_changed_cb (TestData *data, gint frequency)
{
  g_assert_cmpint (frequency, ==, data->target_freq);

  data->target_freq = data->freqs[++(data->idx)];

  if (data->target_freq == 0)
    {
      GST_DEBUG_OBJECT (data->fmsrc, "Tune test completed");
      test_done (data);
    }
  else
    {
      GST_DEBUG_OBJECT (data->fmsrc, "Setting frequency to %i", data->target_freq);
      g_object_set (data->fmsrc, "frequency", data->target_freq, NULL);
    }
}

static void
test_tune ()
{
  GST_DEBUG ("Starting tune test");
  TestData *data = tearup (96700000,
			   test_tune_playing_cb,
			   test_tune_freq_changed_cb,
			   NULL, NULL, NULL, NULL);
  test_run (data);
}

static void
test_seek_up (TestData *data);

static gboolean
seek_wait_cb (void *userdata)
{
  TestData *data = userdata;
  GST_DEBUG_OBJECT (data->fmsrc, "Wait elapsed, resuming seek");
  data->seek_state = SEEK_UP;
  test_seek_up(data);
  return FALSE;
}

static gboolean
seek_start_cb (void *userdata)
{

  TestData *data = userdata;
  GST_DEBUG_OBJECT (data->fmsrc, "End of start period, cancelling seek");

  g_signal_emit_by_name (data->fmsrc, "cancel-seek");
  data->seek_state = SEEK_WAIT;
  data->timeout_id = g_timeout_add_seconds (3, seek_wait_cb, data);
  return FALSE;
}

static void
test_seek_up (TestData *data)
{
  GST_DEBUG_OBJECT (data->fmsrc, "Seeking up for station");
  g_signal_emit_by_name (data->fmsrc, "seek-up");

  if (data->seek_state == SEEK_START)
    data->timeout_id = g_timeout_add_seconds (2, seek_start_cb, data);
}

static void
test_seek_station_found_cb (TestData *data, gint frequency)
{
  GST_DEBUG_OBJECT (data->fmsrc, "Found station at frequency %d in state %d",
    frequency, data->seek_state);

  switch (data->seek_state)
    {
      case SEEK_START:
	g_signal_emit_by_name (data->fmsrc, "seek-up");
	GST_DEBUG_OBJECT (data->fmsrc, "Found station while starting, seeking again");
	break;
   
      case SEEK_UP:
	data->seek_state = SEEK_DOWN;
	g_signal_emit_by_name (data->fmsrc, "seek-down");
	GST_DEBUG_OBJECT (data->fmsrc, "Found station at %d while seeking up, seeking down",
			  frequency);
	break;

      case SEEK_DOWN:
	GST_DEBUG_OBJECT (data->fmsrc, "Found station at %d while seeking down, test complete",
                          frequency);
	data->seek_state = SEEK_DONE;
	test_done (data);
	break;

      case SEEK_DONE:
      case SEEK_WAIT:
	break;
    }
}

static void
test_seek ()
{
  GST_DEBUG ("Starting seek test");
  TestData *data = tearup (88100000,
			   test_seek_up,
			   NULL,
			   test_seek_station_found_cb,
                           NULL, NULL, NULL);
  test_run (data);
}

static const char *
rds_string_type_desc (RDSStringType type)
{
  switch (type)
    {
    case RDS_STATION_LABEL: return "station label";
    case RDS_RADIO_TEXT: return "radio text";
    default: return NULL;
    };
}

static void
test_rds_clear (TestData *data, RDSStringType type)
{
  const char *type_desc = rds_string_type_desc (type);

  if (type != data->rds_type)
    return;

  GST_DEBUG_OBJECT (data->fmsrc, "RDS %s cleared", type_desc);

  switch (data->rds_state)
    {
    case RDS_CLEAR:
      GST_DEBUG_OBJECT (data->fmsrc, "Successfully cleared RDS %s"
			", switching frequency back", type_desc);
      data->rds_state = RDS_SECOND;
      g_object_set (data->fmsrc, "frequency", RDS_CAPABLE_STATION, NULL);
      break;

    case RDS_FIRST:
    case RDS_SECOND:
      break;
  };
}

static void
test_rds_change (TestData *data, RDSStringType type, const gchar *label)
{
  if (type != data->rds_type)
    return;

  GST_DEBUG_OBJECT (data->fmsrc, "RDS %s changed to: `%s'",
		    rds_string_type_desc (type), label);
}

static void
test_rds_complete (TestData *data, RDSStringType type, const gchar *label)
{
  const char *type_desc = rds_string_type_desc (type);

  if (type != data->rds_type)
    return;

  GST_DEBUG_OBJECT (data->fmsrc, "RDS %s complete: `%s'", type_desc, label);

  switch (data->rds_state)
    {
    case RDS_FIRST:
      GST_DEBUG_OBJECT (data->fmsrc, "First RDS %s complete, switching frequency", type_desc);
      data->rds_state = RDS_CLEAR;
      g_object_set (data->fmsrc, "frequency", RDS_CAPABLE_STATION + 1000000, NULL);
      break;
    case RDS_SECOND:
      GST_DEBUG_OBJECT (data->fmsrc, "Second RDS %s complete, test complete", type_desc);
      test_done (data);
      break;

    case RDS_CLEAR:
      break;
  };
}


static void
test_rds (RDSStringType type)
{
  GST_DEBUG ("Starting RDS %s test", rds_string_type_desc (type));
  TestData *data = tearup (RDS_CAPABLE_STATION,
			   NULL,
			   NULL,
			   NULL,
			   test_rds_clear,
			   test_rds_change,
			   test_rds_complete);
  data->timeout = 60 * 60;
  data->rds_type = type;

  test_run (data);
}

static void
test_rds_station_label ()
{
  test_rds (RDS_STATION_LABEL);
}

static void
test_rds_radio_text ()
{
  test_rds (RDS_RADIO_TEXT);
}

static gboolean
test_startstop_second_start_cb (void *userdata)
{
  TestData *data = userdata;
  gint i;

  GST_DEBUG_OBJECT (data->fmsrc, "Second start wait elapsed ...");

  for (i = 0; i < 10; i++) {
    if (i & 1)
      gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
    else
      gst_element_set_state (data->pipeline, GST_STATE_READY);
  }

  test_done (data);
  return FALSE;
}

static gboolean
test_startstop_stop_cb (void *userdata)
{
  TestData *data = userdata;
  GST_DEBUG_OBJECT (data->fmsrc, "Stopping wait elapsed, starting element again");
  data->startstop_state = STARTSTOP_SECOND_START;
  gst_element_set_state (data->pipeline, GST_STATE_PLAYING);
  return FALSE;
}

static gboolean
test_startstop_begin_cb (void *userdata)
{
  TestData *data = userdata;
  GST_DEBUG_OBJECT (data->fmsrc, "Beginning wait elapsed, stopping element");
  data->startstop_state = STARTSTOP_STOP;
  GstStateChangeReturn ret = gst_element_set_state (data->pipeline, GST_STATE_READY);
  if (ret != GST_STATE_CHANGE_SUCCESS)
    GST_ERROR_OBJECT (data->fmsrc, "Setting pipeline state was not successful, return value: %i",
		      (int)ret);
  g_timeout_add_seconds (2, test_startstop_stop_cb, data);
  return FALSE;
}

static void
test_startstop_playing_cb (TestData *data)
{
  GST_DEBUG_OBJECT (data->fmsrc, "Pipeline playing");

  switch (data->startstop_state)
    {
    case STARTSTOP_BEGIN:
      g_timeout_add_seconds (2, test_startstop_begin_cb, data);
      break;
    case STARTSTOP_SECOND_START:
      g_timeout_add_seconds (2, test_startstop_second_start_cb, data);
      break;
    default:
      g_assert_not_reached ();
      break;
    };
}


static void
test_startstop ()
{
  GST_DEBUG ("Starting Start/Stop test");
  TestData *data = tearup (RDS_CAPABLE_STATION,
			   test_startstop_playing_cb,
                           NULL, NULL, NULL, NULL, NULL);

  data->timeout = 300;

  test_run (data);
}

gint
main (gint argc, gchar **argv)
{
  gst_init (&argc, &argv);

  GST_DEBUG_CATEGORY_INIT (sdrjfm_debug, "sdrjfm", 0, "SDR-J FM plugin");
  GST_DEBUG ("Running test");

  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/tune/live", test_tune);
  g_test_add_func ("/tune/seek", test_seek);
  g_test_add_func ("/tune/rds_station_label", test_rds_station_label);
  g_test_add_func ("/tune/rds_radio_text", test_rds_radio_text);
  g_test_add_func ("/tune/startstop", test_startstop);
  g_test_run ();
  return 0;
}
