/* GStreamer
 *
 * Unit tests for webrtcbin
 *
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/check/gstcheck.h>
#include <gst/webrtc/webrtc.h>

#define OPUS_RTP_CAPS(pt) "application/x-rtp,payload=" G_STRINGIFY(pt) ",encoding-name=OPUS,media=audio,clock-rate=48000"
#define VP8_RTP_CAPS(pt) "application/x-rtp,payload=" G_STRINGIFY(pt) ",encoding-name=VP8,media=video,clock-rate=90000"

typedef enum
{
  STATE_NEW,
  STATE_NEGOTATION_NEEDED,
  STATE_OFFER_CREATED,
  STATE_ANSWER_CREATED,
  STATE_EOS,
  STATE_ERROR,
} TestState;

/* basic premise of this is that webrtc1 and webrtc2 are attempting to connect
 * to each other in various configurations */
struct test_webrtc;
struct test_webrtc
{
  GThread *thread;
  GMainLoop *loop;
  GstElement *pipeline;
  GstElement *webrtc1;
  GstElement *webrtc2;
  GMutex lock;
  GCond cond;
  TestState state;
  guint offerror;
  gpointer user_data;
  GDestroyNotify data_notify;
/* *INDENT-OFF* */
  void      (*on_negotiation_needed)    (struct test_webrtc * t,
                                         GstElement * element,
                                         gpointer user_data);
  gpointer negotiation_data;
  GDestroyNotify negotiation_notify;
  void      (*on_ice_candidate)         (struct test_webrtc * t,
                                         GstElement * element,
                                         guint mlineindex,
                                         gchar * candidate,
                                         GstElement * other,
                                         gpointer user_data);
  gpointer ice_candidate_data;
  GDestroyNotify ice_candidate_notify;
  GstWebRTCSessionDescription * (*on_offer_created)     (struct test_webrtc * t,
                                                         GstElement * element,
                                                         GstPromise * promise,
                                                         gpointer user_data);
  gpointer offer_data;
  GDestroyNotify offer_notify;
  GstWebRTCSessionDescription * (*on_answer_created)    (struct test_webrtc * t,
                                                         GstElement * element,
                                                         GstPromise * promise,
                                                         gpointer user_data);
  gpointer answer_data;
  GDestroyNotify answer_notify;
  void      (*on_pad_added)             (struct test_webrtc * t,
                                         GstElement * element,
                                         GstPad * pad,
                                         gpointer user_data);
  gpointer pad_added_data;
  GDestroyNotify pad_added_notify;
  void      (*bus_message)              (struct test_webrtc * t,
                                         GstBus * bus,
                                         GstMessage * msg,
                                         gpointer user_data);
  gpointer bus_data;
  GDestroyNotify bus_notify;
/* *INDENT-ON* */
};

static void
_on_answer_received (GstPromise * promise, gpointer user_data)
{
  struct test_webrtc *t = user_data;
  GstElement *offeror = t->offerror == 1 ? t->webrtc1 : t->webrtc2;
  GstElement *answerer = t->offerror == 2 ? t->webrtc1 : t->webrtc2;
  GstWebRTCSessionDescription *answer = NULL;
  gchar *desc;

  gst_structure_get (promise->promise, "answer",
      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
  desc = gst_sdp_message_as_text (answer->sdp);
  GST_INFO ("Created Answer: %s", desc);
  g_free (desc);

  g_mutex_lock (&t->lock);
  if (t->on_offer_created) {
    gst_webrtc_session_description_free (answer);
    answer = t->on_answer_created (t, answerer, promise, t->answer_data);
  }
  gst_promise_unref (promise);

  g_signal_emit_by_name (answerer, "set-local-description", answer, NULL);
  g_signal_emit_by_name (offeror, "set-remote-description", answer, NULL);

  t->state = STATE_ANSWER_CREATED;
  g_cond_broadcast (&t->cond);
  g_mutex_unlock (&t->lock);

  gst_webrtc_session_description_free (answer);
}

static void
_on_offer_received (GstPromise * promise, gpointer user_data)
{
  struct test_webrtc *t = user_data;
  GstElement *offeror = t->offerror == 1 ? t->webrtc1 : t->webrtc2;
  GstElement *answerer = t->offerror == 2 ? t->webrtc1 : t->webrtc2;
  GstWebRTCSessionDescription *offer = NULL;
  gchar *desc;

  gst_structure_get (promise->promise, "offer",
      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
  desc = gst_sdp_message_as_text (offer->sdp);
  GST_INFO ("Created offer: %s", desc);
  g_free (desc);

  g_mutex_lock (&t->lock);
  if (t->on_offer_created) {
    gst_webrtc_session_description_free (offer);
    offer = t->on_offer_created (t, offeror, promise, t->offer_data);
  }
  gst_promise_unref (promise);

  g_signal_emit_by_name (offeror, "set-local-description", offer, NULL);
  g_signal_emit_by_name (answerer, "set-remote-description", offer, NULL);

  promise = gst_promise_new ();
  gst_promise_set_change_callback (promise, _on_answer_received, t, NULL);
  g_signal_emit_by_name (answerer, "create-answer", NULL, promise);

  t->state = STATE_OFFER_CREATED;
  g_cond_broadcast (&t->cond);
  g_mutex_unlock (&t->lock);

  gst_webrtc_session_description_free (offer);
}

static gboolean
_bus_watch (GstBus * bus, GstMessage * msg, struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_STATE_CHANGED:
      if (GST_ELEMENT (msg->src) == t->pipeline) {
        GstState old, new, pending;

        gst_message_parse_state_changed (msg, &old, &new, &pending);

        {
          gchar *dump_name = g_strconcat ("state_changed-",
              gst_element_state_get_name (old), "_",
              gst_element_state_get_name (new), NULL);
          GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (msg->src),
              GST_DEBUG_GRAPH_SHOW_ALL, dump_name);
          g_free (dump_name);
        }
      }
      break;
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *dbg_info = NULL;

      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (t->pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "error");

      gst_message_parse_error (msg, &err, &dbg_info);
      GST_WARNING ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), err->message);
      GST_WARNING ("Debugging info: %s\n", (dbg_info) ? dbg_info : "none");
      g_error_free (err);
      g_free (dbg_info);
      t->state = STATE_ERROR;
      g_cond_broadcast (&t->cond);
      break;
    }
    case GST_MESSAGE_EOS:{
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (t->pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "eos");
      GST_INFO ("EOS received\n");
      t->state = STATE_EOS;
      g_cond_broadcast (&t->cond);
      break;
    }
    default:
      break;
  }

  if (t->bus_message)
    t->bus_message (t, bus, msg, t->bus_data);
  g_mutex_unlock (&t->lock);

  return TRUE;
}

static void
_on_negotiation_needed (GstElement * webrtc, struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  if (t->on_negotiation_needed)
    t->on_negotiation_needed (t, webrtc, t->negotiation_data);
  if (t->state == STATE_NEW)
    t->state = STATE_NEGOTATION_NEEDED;
  g_cond_broadcast (&t->cond);
  g_mutex_unlock (&t->lock);
}

static void
_on_ice_candidate (GstElement * webrtc, guint mlineindex, gchar * candidate,
    struct test_webrtc *t)
{
  GstElement *other;

  g_mutex_lock (&t->lock);
  other = webrtc == t->webrtc1 ? t->webrtc2 : t->webrtc1;

  if (t->on_ice_candidate)
    t->on_ice_candidate (t, webrtc, mlineindex, candidate, other,
        t->ice_candidate_data);

  g_signal_emit_by_name (other, "add-ice-candidate", mlineindex, candidate);
  g_mutex_unlock (&t->lock);
}

static void
_on_pad_added (GstElement * webrtc, GstPad * new_pad, struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  if (t->on_pad_added)
    t->on_pad_added (t, webrtc, new_pad, t->pad_added_data);
  g_mutex_unlock (&t->lock);
}

static void
_pad_added_not_reached (struct test_webrtc *t, GstElement * element,
    GstPad * pad, gpointer user_data)
{
  g_assert_not_reached ();
}

static void
_ice_candidate_not_reached (struct test_webrtc *t, GstElement * element,
    guint mlineindex, gchar * candidate, GstElement * other, gpointer user_data)
{
  g_assert_not_reached ();
}

static void
_negotiation_not_reached (struct test_webrtc *t, GstElement * element,
    gpointer user_data)
{
  g_assert_not_reached ();
}

static void
_bus_no_errors (struct test_webrtc *t, GstBus * bus, GstMessage * msg,
    gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      g_assert_not_reached ();
      break;
    }
    default:
      break;
  }
}

static GstWebRTCSessionDescription *
_offer_answer_not_reached (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
  g_assert_not_reached ();
}

static void
_broadcast (struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  g_cond_broadcast (&t->cond);
  g_mutex_unlock (&t->lock);
}

static gboolean
_unlock_create_thread (GMutex * lock)
{
  g_mutex_unlock (lock);
  return G_SOURCE_REMOVE;
}

static gpointer
_bus_thread (struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  t->loop = g_main_loop_new (NULL, FALSE);
  g_idle_add ((GSourceFunc) _unlock_create_thread, &t->lock);
  g_cond_broadcast (&t->cond);

  g_main_loop_run (t->loop);

  g_mutex_lock (&t->lock);
  g_main_loop_unref (t->loop);
  t->loop = NULL;
  g_cond_broadcast (&t->cond);
  g_mutex_unlock (&t->lock);

  return NULL;
}

static struct test_webrtc *
test_webrtc_new (void)
{
  struct test_webrtc *ret = g_new0 (struct test_webrtc, 1);
  GstBus *bus;

  ret->on_negotiation_needed = _negotiation_not_reached;
  ret->on_ice_candidate = _ice_candidate_not_reached;
  ret->on_pad_added = _pad_added_not_reached;
  ret->on_offer_created = _offer_answer_not_reached;
  ret->on_answer_created = _offer_answer_not_reached;
  ret->bus_message = _bus_no_errors;

  g_mutex_init (&ret->lock);
  g_cond_init (&ret->cond);

  ret->pipeline = gst_pipeline_new (NULL);
  bus = gst_pipeline_get_bus (GST_PIPELINE (ret->pipeline));
  gst_bus_add_watch (bus, (GstBusFunc) _bus_watch, ret);
  gst_object_unref (bus);
  ret->webrtc1 = gst_element_factory_make ("webrtcbin", NULL);
  ret->webrtc2 = gst_element_factory_make ("webrtcbin", NULL);
  fail_unless (ret->webrtc1 != NULL && ret->webrtc2 != NULL);

  gst_bin_add (GST_BIN (ret->pipeline), ret->webrtc1);
  gst_bin_add (GST_BIN (ret->pipeline), ret->webrtc2);

  g_signal_connect (ret->webrtc1, "on-negotiation-needed",
      G_CALLBACK (_on_negotiation_needed), ret);
  g_signal_connect (ret->webrtc2, "on-negotiation-needed",
      G_CALLBACK (_on_negotiation_needed), ret);
  g_signal_connect (ret->webrtc1, "on-ice-candidate",
      G_CALLBACK (_on_ice_candidate), ret);
  g_signal_connect (ret->webrtc2, "on-ice-candidate",
      G_CALLBACK (_on_ice_candidate), ret);
  g_signal_connect (ret->webrtc1, "pad-added", G_CALLBACK (_on_pad_added), ret);
  g_signal_connect (ret->webrtc2, "pad-added", G_CALLBACK (_on_pad_added), ret);
  g_signal_connect_swapped (ret->webrtc1, "notify::ice-gathering-state",
      G_CALLBACK (_broadcast), ret);
  g_signal_connect_swapped (ret->webrtc2, "notify::ice-gathering-state",
      G_CALLBACK (_broadcast), ret);
  g_signal_connect_swapped (ret->webrtc1, "notify::ice-connection-state",
      G_CALLBACK (_broadcast), ret);
  g_signal_connect_swapped (ret->webrtc2, "notify::ice-connection-state",
      G_CALLBACK (_broadcast), ret);

  ret->thread = g_thread_new ("test-webrtc", (GThreadFunc) _bus_thread, ret);

  g_mutex_lock (&ret->lock);
  while (!ret->loop)
    g_cond_wait (&ret->cond, &ret->lock);
  g_mutex_unlock (&ret->lock);

  return ret;
}

static void
test_webrtc_free (struct test_webrtc *t)
{
  gst_element_set_state (t->pipeline, GST_STATE_NULL);

  /* Otherwise while one webrtcbin is being destroyed, the other could
   * generate a signal that calls into the destroyed webrtcbin */
  g_signal_handlers_disconnect_by_data (t->webrtc1, t);
  g_signal_handlers_disconnect_by_data (t->webrtc2, t);

  g_main_loop_quit (t->loop);
  g_mutex_lock (&t->lock);
  while (t->loop)
    g_cond_wait (&t->cond, &t->lock);
  g_mutex_unlock (&t->lock);

  g_thread_join (t->thread);

  gst_object_unref (t->pipeline);

  if (t->data_notify)
    t->data_notify (t->user_data);
  if (t->negotiation_notify)
    t->negotiation_notify (t->negotiation_data);
  if (t->ice_candidate_notify)
    t->ice_candidate_notify (t->ice_candidate_data);
  if (t->offer_notify)
    t->offer_notify (t->offer_data);
  if (t->answer_notify)
    t->answer_notify (t->answer_data);
  if (t->pad_added_notify)
    t->pad_added_notify (t->pad_added_data);

  g_mutex_clear (&t->lock);
  g_cond_clear (&t->cond);

  g_free (t);
}

static void
test_webrtc_create_offer (struct test_webrtc *t, GstElement * webrtc)
{
  GstPromise *promise = gst_promise_new ();

  t->offerror = webrtc == t->webrtc1 ? 1 : 2;
  gst_promise_set_change_callback (promise, _on_offer_received, t, NULL);
  g_signal_emit_by_name (webrtc, "create-offer", NULL, promise);
}

static void
test_webrtc_wait_for_answer_error_eos (struct test_webrtc *t)
{
  g_mutex_lock (&t->lock);
  while (t->state != STATE_ANSWER_CREATED && t->state != STATE_EOS
      && t->state != STATE_ERROR)
    g_cond_wait (&t->cond, &t->lock);
  g_mutex_unlock (&t->lock);
}

#if 0
static void
test_webrtc_wait_for_ice_gathering_complete (struct test_webrtc *t)
{
  GstWebRTCICEGatheringState ice_state1, ice_state2;
  g_mutex_lock (&t->lock);
  g_object_get (t->webrtc1, "ice-gathering-state", &ice_state1, NULL);
  g_object_get (t->webrtc2, "ice-gathering-state", &ice_state2, NULL);
  while (ice_state1 != GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE &&
      ice_state2 != GST_WEBRTC_ICE_GATHERING_STATE_COMPLETE) {
    g_cond_wait (&t->cond, &t->lock);
    g_object_get (t->webrtc1, "ice-gathering-state", &ice_state1, NULL);
    g_object_get (t->webrtc2, "ice-gathering-state", &ice_state2, NULL);
  }
  g_mutex_unlock (&t->lock);
}

static void
test_webrtc_wait_for_ice_connection (struct test_webrtc *t,
    GstWebRTCICEConnectionState states)
{
  GstWebRTCICEConnectionState ice_state1, ice_state2, current;
  g_mutex_lock (&t->lock);
  g_object_get (t->webrtc1, "ice-connection-state", &ice_state1, NULL);
  g_object_get (t->webrtc2, "ice-connection-state", &ice_state2, NULL);
  current = (1 << ice_state1) | (1 << ice_state2);
  while ((current & states) == 0 || (current & ~states)) {
    g_cond_wait (&t->cond, &t->lock);
    g_object_get (t->webrtc1, "ice-connection-state", &ice_state1, NULL);
    g_object_get (t->webrtc2, "ice-connection-state", &ice_state2, NULL);
    current = (1 << ice_state1) | (1 << ice_state2);
  }
  g_mutex_unlock (&t->lock);
}
#endif
static void
_pad_added_fakesink (struct test_webrtc *t, GstElement * element,
    GstPad * pad, gpointer user_data)
{
  GstElement *fakesink;
  GstPad *sink;

  if (GST_PAD_DIRECTION (pad) != GST_PAD_SRC)
    return;

  fakesink = gst_element_factory_make ("fakesink", NULL);
  g_object_set (fakesink, "async", FALSE, "sync", FALSE, NULL);
  gst_bin_add (GST_BIN (t->pipeline), fakesink);

  sink = fakesink->sinkpads->data;

  gst_pad_link (pad, sink);
}

static GstWebRTCSessionDescription *
_count_num_sdp_media (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
  GstWebRTCSessionDescription *offer = NULL;
  guint expected = GPOINTER_TO_UINT (user_data);
  const gchar *field;

  field = t->offerror == 1 && t->webrtc1 == element ? "offer" : "answer";

  gst_structure_get (promise->promise, field,
      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);

  fail_unless_equals_int (gst_sdp_message_medias_len (offer->sdp), expected);

  return offer;
}

GST_START_TEST (test_sdp_no_media)
{
  struct test_webrtc *t = test_webrtc_new ();

  t->offer_data = GUINT_TO_POINTER (0);
  t->on_offer_created = _count_num_sdp_media;
  t->answer_data = GUINT_TO_POINTER (0);
  t->on_answer_created = _count_num_sdp_media;

  test_webrtc_create_offer (t, t->webrtc1);

  test_webrtc_wait_for_answer_error_eos (t);
  fail_unless (t->state == STATE_ANSWER_CREATED);
  test_webrtc_free (t);
}

GST_END_TEST;

static struct test_webrtc *
create_audio_test (void)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstElement *src;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  src =
      gst_parse_bin_from_description ("fakesrc ! capsfilter caps="
      OPUS_RTP_CAPS (96), TRUE, NULL);
  fail_unless (src != NULL, "Could not create input pipeline");
  fail_unless (gst_bin_add (GST_BIN (t->pipeline), src));
  fail_unless (gst_element_link (src, t->webrtc1));

  return t;
}

GST_START_TEST (test_audio)
{
  struct test_webrtc *t = create_audio_test ();

  t->offer_data = GUINT_TO_POINTER (1);
  t->on_offer_created = _count_num_sdp_media;
  t->answer_data = GUINT_TO_POINTER (1);
  t->on_answer_created = _count_num_sdp_media;

  test_webrtc_create_offer (t, t->webrtc1);

  test_webrtc_wait_for_answer_error_eos (t);
  fail_unless_equals_int (STATE_ANSWER_CREATED, t->state);
  test_webrtc_free (t);
}

GST_END_TEST;

static struct test_webrtc *
create_audio_video_test (void)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstElement *src;

  t->on_negotiation_needed = NULL;
  t->on_ice_candidate = NULL;
  t->on_pad_added = _pad_added_fakesink;

  src =
      gst_parse_bin_from_description ("fakesrc ! capsfilter caps="
      OPUS_RTP_CAPS (96), TRUE, NULL);
  fail_unless (src != NULL, "Could not create input pipeline");
  fail_unless (gst_bin_add (GST_BIN (t->pipeline), src));
  fail_unless (gst_element_link (src, t->webrtc1));

  src =
      gst_parse_bin_from_description ("fakesrc ! capsfilter caps="
      VP8_RTP_CAPS (97), TRUE, NULL);
  fail_unless (src != NULL, "Could not create input pipeline");
  fail_unless (gst_bin_add (GST_BIN (t->pipeline), src));
  fail_unless (gst_element_link (src, t->webrtc1));

  return t;
}

GST_START_TEST (test_audio_video)
{
  struct test_webrtc *t = create_audio_video_test ();

  t->offer_data = GUINT_TO_POINTER (2);
  t->on_offer_created = _count_num_sdp_media;
  t->answer_data = GUINT_TO_POINTER (2);
  t->on_answer_created = _count_num_sdp_media;

  test_webrtc_create_offer (t, t->webrtc1);

  test_webrtc_wait_for_answer_error_eos (t);
  fail_unless_equals_int (STATE_ANSWER_CREATED, t->state);
  test_webrtc_free (t);
}

GST_END_TEST;

typedef void (*ValidateSDPFunc) (struct test_webrtc * t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data);

struct validate_sdp
{
  ValidateSDPFunc validate;
  gpointer user_data;
};

static GstWebRTCSessionDescription *
validate_sdp (struct test_webrtc *t, GstElement * element,
    GstPromise * promise, gpointer user_data)
{
  struct validate_sdp *validate = user_data;
  GstWebRTCSessionDescription *offer = NULL;
  const gchar *field;

  field = t->offerror == 1 && t->webrtc1 == element ? "offer" : "answer";

  gst_structure_get (promise->promise, field,
      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);

  validate->validate (t, element, offer, validate->user_data);

  return offer;
}

static void
on_sdp_media_direction (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  gchar **expected_directions = user_data;
  int i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);
    gboolean have_direction = FALSE;
    int j;

    for (j = 0; j < gst_sdp_media_attributes_len (media); j++) {
      const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, j);

      if (g_strcmp0 (attr->key, "inactive") == 0) {
        fail_unless (have_direction == FALSE,
            "duplicate/multiple directions for media %u", j);
        have_direction = TRUE;
        fail_unless (g_strcmp0 (attr->key, expected_directions[i]) == 0);
      } else if (g_strcmp0 (attr->key, "sendonly") == 0) {
        fail_unless (have_direction == FALSE,
            "duplicate/multiple directions for media %u", j);
        have_direction = TRUE;
        fail_unless (g_strcmp0 (attr->key, expected_directions[i]) == 0);
      } else if (g_strcmp0 (attr->key, "recvonly") == 0) {
        fail_unless (have_direction == FALSE,
            "duplicate/multiple directions for media %u", j);
        have_direction = TRUE;
        fail_unless (g_strcmp0 (attr->key, expected_directions[i]) == 0);
      } else if (g_strcmp0 (attr->key, "sendrecv") == 0) {
        fail_unless (have_direction == FALSE,
            "duplicate/multiple directions for media %u", j);
        have_direction = TRUE;
        fail_unless (g_strcmp0 (attr->key, expected_directions[i]) == 0);
      }
    }
    fail_unless (have_direction, "no direction attribute in media %u", j);
  }
}

GST_START_TEST (test_media_direction)
{
  struct test_webrtc *t = create_audio_video_test ();
  const gchar *expected_offer[] = { "sendrecv", "sendrecv" };
  const gchar *expected_answer[] = { "sendrecv", "recvonly" };
  struct validate_sdp offer = { on_sdp_media_direction, expected_offer };
  struct validate_sdp answer = { on_sdp_media_direction, expected_answer };
  GstElement *src;

  src =
      gst_parse_bin_from_description ("fakesrc ! capsfilter caps="
      OPUS_RTP_CAPS (96), TRUE, NULL);
  fail_unless (src != NULL, "Could not create input pipeline");
  fail_unless (gst_bin_add (GST_BIN (t->pipeline), src));
  fail_unless (gst_element_link (src, t->webrtc2));

  t->offer_data = &offer;
  t->on_offer_created = validate_sdp;
  t->answer_data = &answer;
  t->on_answer_created = validate_sdp;

  test_webrtc_create_offer (t, t->webrtc1);

  test_webrtc_wait_for_answer_error_eos (t);
  fail_unless_equals_int (STATE_ANSWER_CREATED, t->state);
  test_webrtc_free (t);
}

GST_END_TEST;

static void
on_sdp_media_setup (struct test_webrtc *t, GstElement * element,
    GstWebRTCSessionDescription * desc, gpointer user_data)
{
  gchar **expected_setup = user_data;
  int i;

  for (i = 0; i < gst_sdp_message_medias_len (desc->sdp); i++) {
    const GstSDPMedia *media = gst_sdp_message_get_media (desc->sdp, i);
    gboolean have_setup = FALSE;
    int j;

    for (j = 0; j < gst_sdp_media_attributes_len (media); j++) {
      const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, j);

      if (g_strcmp0 (attr->key, "setup") == 0) {
        fail_unless (have_setup == FALSE,
            "duplicate/multiple setup for media %u", j);
        have_setup = TRUE;
        fail_unless (g_strcmp0 (attr->value, expected_setup[i]) == 0);
      }
    }
    fail_unless (have_setup, "no setup attribute in media %u", j);
  }
}

GST_START_TEST (test_media_setup)
{
  struct test_webrtc *t = create_audio_test ();
  const gchar *expected_offer[] = { "actpass" };
  const gchar *expected_answer[] = { "active" };
  struct validate_sdp offer = { on_sdp_media_setup, expected_offer };
  struct validate_sdp answer = { on_sdp_media_setup, expected_answer };

  t->offer_data = &offer;
  t->on_offer_created = validate_sdp;
  t->answer_data = &answer;
  t->on_answer_created = validate_sdp;

  test_webrtc_create_offer (t, t->webrtc1);

  test_webrtc_wait_for_answer_error_eos (t);
  fail_unless_equals_int (STATE_ANSWER_CREATED, t->state);
  test_webrtc_free (t);
}

GST_END_TEST;

GST_START_TEST (test_no_nice_elements_request_pad)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstPluginFeature *nicesrc, *nicesink;
  GstRegistry *registry;
  GstPad *pad;

  registry = gst_registry_get ();
  nicesrc = gst_registry_lookup_feature (registry, "nicesrc");
  nicesink = gst_registry_lookup_feature (registry, "nicesink");

  if (nicesrc)
    gst_registry_remove_feature (registry, nicesrc);
  if (nicesink)
    gst_registry_remove_feature (registry, nicesink);

  t->bus_message = NULL;

  pad = gst_element_get_request_pad (t->webrtc1, "sink_0");
  fail_unless (pad == NULL);

  test_webrtc_wait_for_answer_error_eos (t);
  fail_unless_equals_int (STATE_ERROR, t->state);
  test_webrtc_free (t);

  if (nicesrc)
    gst_registry_add_feature (registry, nicesrc);
  if (nicesink)
    gst_registry_add_feature (registry, nicesink);
}

GST_END_TEST;

GST_START_TEST (test_no_nice_elements_state_change)
{
  struct test_webrtc *t = test_webrtc_new ();
  GstPluginFeature *nicesrc, *nicesink;
  GstRegistry *registry;

  registry = gst_registry_get ();
  nicesrc = gst_registry_lookup_feature (registry, "nicesrc");
  nicesink = gst_registry_lookup_feature (registry, "nicesink");

  if (nicesrc)
    gst_registry_remove_feature (registry, nicesrc);
  if (nicesink)
    gst_registry_remove_feature (registry, nicesink);

  t->bus_message = NULL;

  fail_unless_equals_int (GST_STATE_CHANGE_FAILURE,
      gst_element_set_state (t->pipeline, GST_STATE_READY));

  test_webrtc_wait_for_answer_error_eos (t);
  fail_unless_equals_int (STATE_ERROR, t->state);
  test_webrtc_free (t);

  if (nicesrc)
    gst_registry_add_feature (registry, nicesrc);
  if (nicesink)
    gst_registry_add_feature (registry, nicesink);
}

GST_END_TEST;

static Suite *
webrtcbin_suite (void)
{
  Suite *s = suite_create ("webrtcbin");
  TCase *tc = tcase_create ("general");
  GstPluginFeature *nicesrc, *nicesink;
  GstRegistry *registry;

  registry = gst_registry_get ();
  nicesrc = gst_registry_lookup_feature (registry, "nicesrc");
  nicesink = gst_registry_lookup_feature (registry, "nicesink");

  tcase_add_test (tc, test_sdp_no_media);
  tcase_add_test (tc, test_no_nice_elements_request_pad);
  tcase_add_test (tc, test_no_nice_elements_state_change);
  if (nicesrc && nicesink) {
    tcase_add_test (tc, test_audio);
    tcase_add_test (tc, test_audio_video);
    tcase_add_test (tc, test_media_direction);
    tcase_add_test (tc, test_media_setup);
  }

  if (nicesrc)
    gst_object_unref (nicesrc);
  if (nicesink)
    gst_object_unref (nicesink);

  suite_add_tcase (s, tc);

  return s;
}

GST_CHECK_MAIN (webrtcbin);
