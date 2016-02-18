#include <glib.h>
#include <gst/gst.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY (sdrjfm_optimise_debug);
#define GST_CAT_DEFAULT sdrjfm_optimise_debug

#define KNOWN_GOOD_STATION_FREQUENCY 105900000
#define START_OFFSET (-1000000)
#define MAX_GAP_WITHOUT_STATION 500000
#define CONFIRMATIONS 9
#define SEEK_DELAY 333

typedef struct _OptimisationParameters OptimisationParameters;
struct _OptimisationParameters
{
  const char *name;
  gint start;
  gint end;
  gint step;
};

const OptimisationParameters OPTIMISATION_PARAMETERS[] = 
  { { "threshold", 25, 50, 1 },
    { "interval", 500, 5, -20 },
    { NULL, 0, 0, 0 }
  };

typedef struct _TestData TestData;
struct _TestData
{
  GMainLoop *loop;
  GstElement *pipeline;
  GstElement *fmsrc;
  gint target;
  const char *seek_signal_name;
  gint optimisation_index;

  gint best;
  gint current_count;
};

static const OptimisationParameters *
get_current_params(TestData *data)
{
  return &OPTIMISATION_PARAMETERS[data->optimisation_index];
}

static gboolean
do_seek (void *user_data)
{
  TestData *data = user_data;

  const gint start_freq = data->target + START_OFFSET;
  g_object_set (data->fmsrc, "frequency", start_freq, NULL);

  g_signal_emit_by_name (data->fmsrc, data->seek_signal_name);

  return FALSE;
}

static void
seek (TestData *data)
{
  // This is a delay because the hardware has issues if we hammer it
  g_timeout_add (SEEK_DELAY, do_seek, data);
}

static void
optimise (TestData *data)
{
  const OptimisationParameters * params = get_current_params (data);

  GST_DEBUG_OBJECT (data->fmsrc, "Starting optimisation %i for parameter `%s' with value %i",
		    data->optimisation_index, params->name, params->start);

  data->best = -1;
  data->current_count = 0;

  g_object_set (data->fmsrc, params->name, params->start, NULL);

  seek (data);
}

static void
next_optimisation (TestData *data)
{
  ++data->optimisation_index;

  const OptimisationParameters * params = get_current_params (data);
  if (!params->name)
    {
      // Done
      g_main_loop_quit (data->loop);
      return;
    }

  optimise(data);
}

static void
frequency_changed (TestData *data, gint frequency)
{
  if ((START_OFFSET > 0 && frequency < (data->target - MAX_GAP_WITHOUT_STATION))
      || (START_OFFSET < 0 && frequency > (data->target + MAX_GAP_WITHOUT_STATION)))
    {
      GST_DEBUG_OBJECT (data->fmsrc, "Seek frequency %i missed target %i; station not found",
			frequency, data->target);

      g_signal_emit_by_name (data->fmsrc, "cancel-seek");

      // We previously found the best value
      const OptimisationParameters * params = get_current_params (data);
      if (data->best == -1)
	{
	  fprintf(stderr, "Could not determine optimum value for property `%s': "
		  "no reliable values found\n", params->name);
	  exit(1);
	}

      g_object_set (data->fmsrc, params->name, data->best, NULL);
      printf("Best value for property `%s': %i\n", params->name, data->best);

      next_optimisation (data);
    }
}

static void
iterate (TestData *data, const OptimisationParameters * params)
{
  // Iterate to the next optimisation value
  gint value;
  g_object_get (data->fmsrc, params->name, &value, NULL);
  gint new_value = value + params->step;

  if ((params->step > 0 && new_value > params->end)
      || (params->step < 0 && new_value < params->end))
    {
      printf ("Optimisation of value `%s' failed\n", params->name);
      next_optimisation (data);
      return;
    }

  g_object_set (data->fmsrc, params->name, new_value, NULL);
  data->current_count = 0;
  GST_DEBUG_OBJECT (data->fmsrc, "Iterated property `%s' to value %i",
		    params->name, new_value);
  seek (data);
}

static void
station_found (TestData *data, gint frequency)
{
  gint current_value;
  GST_DEBUG_OBJECT (data->fmsrc, "Found station at frequency %i", frequency);

  const OptimisationParameters * params = get_current_params (data);

  if (frequency != data->target)
    {
      // This value is too permissive
      GST_DEBUG_OBJECT (data->fmsrc,
			"Station is at early non-target frequency %i"
			", resetting best and skipping to next iteration",
			frequency);
      data->best = -1;
      iterate (data, params);
      return;
    }

  g_object_get (data->fmsrc, params->name, &current_value, NULL);
  ++data->current_count;
  if (data->current_count >= CONFIRMATIONS)
    {
      data->best = current_value;
      GST_DEBUG_OBJECT (data->fmsrc, "New best value for `%s': %i", params->name, data->best);
      iterate (data, params);
    }
  else
    {
      GST_DEBUG_OBJECT (data->fmsrc, "Successful seek count for `%s' value %d: %i",
			params->name, current_value, data->current_count);
      seek (data);
    }
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
	  optimise (data);
      }
      break;

    case GST_MESSAGE_ELEMENT:
      if (GST_MESSAGE_SRC (message) == GST_OBJECT (data->fmsrc)) {
        const GstStructure *s = gst_message_get_structure (message);

        g_assert (gst_structure_has_field_typed (s, "frequency", G_TYPE_INT));

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
  TestData *data = g_slice_new0 (TestData);
  GstBus *bus;

  data->pipeline =
      gst_parse_launch ("sdrjfmsrc name=fmsrc ! pulsesink",
      &error);
  g_assert_no_error (error);
  g_assert(data->pipeline != NULL);

  data->fmsrc = gst_bin_get_by_name (GST_BIN (data->pipeline), "fmsrc");
  g_assert(data->fmsrc != NULL);

  data->target = KNOWN_GOOD_STATION_FREQUENCY;
  data->optimisation_index = 0;
  if (START_OFFSET > 0)
    data->seek_signal_name = "seek_down";
  else
    data->seek_signal_name = "seek_up";

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

gint
main (gint argc, gchar **argv)
{
  TestData *data;

  gst_init (&argc, &argv);

  GST_DEBUG_CATEGORY_INIT (sdrjfm_optimise_debug, "sdrjfm-optimise", 0,
    "Program to optimise seek properties for SDR-J FM Receiver plugin");

  data = tearup();
  g_main_loop_run (data->loop);
  teardown (data);

  return 0;
}
