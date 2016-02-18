#include <glib.h>
#include <gst/gst.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY (sdrjfm_seek_debug);
#define GST_CAT_DEFAULT sdrjfm_seek_debug

#define SEEK_DELAY 333

typedef struct _TestData TestData;
struct _TestData
{
  GMainLoop *loop;
  GstElement *pipeline;
  GstElement *fmsrc;
};

static TestData *evil_data;

static gboolean
do_seek (void *user_data)
{
  TestData *data = user_data;

  g_signal_emit_by_name (data->fmsrc, "seek-up");

  return FALSE;
}

static void
seek (TestData *data)
{
  do_seek(data);

  //g_timeout_add (SEEK_DELAY, do_seek, data);
}

static void
frequency_changed (TestData *data, gint frequency)
{
  GST_DEBUG_OBJECT (data->fmsrc, "Radio frequency changed to %i", frequency);
}

static void
station_found (TestData *data, gint frequency)
{
  GST_DEBUG_OBJECT (data->fmsrc, "Found station at frequency %i", frequency);

  seek (data);
}


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
        if (state == GST_STATE_PLAYING)
	  seek (data);
      }
      break;

    case GST_MESSAGE_ELEMENT:
      if (GST_MESSAGE_SRC (message) == GST_OBJECT (data->fmsrc)) {
        const GstStructure *s = gst_message_get_structure (message);

	if (!gst_structure_has_field_typed (s, "frequency", G_TYPE_INT))
	  break;

	gint freq;
	gst_structure_get_int (s, "frequency", &freq);

	if (gst_structure_has_name (s, "sdrjfmsrc-frequency-changed")) {
	  frequency_changed (data, freq);
	} else if (gst_structure_has_name (s, "sdrjfmsrc-station-found")) {
          station_found (data, freq);
	}
      }
      break;

    case GST_MESSAGE_ERROR:
      gst_message_parse_error (message, &error, NULL);
      g_assert_no_error (error);
      break;

    case GST_MESSAGE_WARNING:
      gst_message_parse_warning (message, &error, NULL);
      g_assert_no_error (error);
      break;

    default:
      break;
  }

  return TRUE;
}

static TestData *
tearup ()
{
  GError *error = NULL;
  TestData *data;
  evil_data = data = g_slice_new0 (TestData);
  GstBus *bus;

  data->pipeline =
      gst_parse_launch ("sdrjfmsrc name=fmsrc ! pulsesink",
      &error);
  g_assert_no_error (error);
  g_assert(data->pipeline != NULL);

  data->fmsrc = gst_bin_get_by_name (GST_BIN (data->pipeline), "fmsrc");
  g_assert(data->fmsrc != NULL);

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
wait_cb (void *userdata)
{
  TestData *data = userdata;
  GST_DEBUG_OBJECT (data->fmsrc, "Wait elapsed, resuming seek");
  do_seek (data);
  return FALSE;
}

static void
signal_handler(int signum)
{
  GST_DEBUG_OBJECT (evil_data->fmsrc, "Signal received, cancelling seek");
  g_signal_emit_by_name (evil_data->fmsrc, "cancel-seek");

  //GST_DEBUG_OBJECT (evil_data->fmsrc, "Seek cancelled, setting frequency");
  //g_object_set (evil_data->fmsrc, "frequency", 88500000, NULL);

  GST_DEBUG_OBJECT (evil_data->fmsrc, "Waiting 3 seconds");
  g_timeout_add_seconds (3, wait_cb, evil_data);
}

static void
signal_init()
{
  int err;
  struct sigaction act;

  memset (&act, 0, sizeof(act));
  act.sa_handler = &signal_handler;

  err = sigaction(SIGHUP, &act, NULL);
  if (err != 0)
    {
      perror("Could not install signal handler");
      exit(1);
    }
}

gint
main (gint argc, gchar **argv)
{
  TestData *data;

  gst_init (&argc, &argv);

  GST_DEBUG_CATEGORY_INIT (sdrjfm_seek_debug, "sdrjfm-seek", 0,
    "Program to test seeking with SDR-J FM Receiver plugin");

  signal_init();

  data = tearup();
  g_main_loop_run (data->loop);
  teardown (data);

  return 0;
}
