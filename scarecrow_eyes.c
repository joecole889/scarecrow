#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/app/gstappsink.h>

#include "scarecrow-target.h"
#include "scarecrow-media-factory.h"

static void media_configure(GstRTSPMediaFactory*, GstRTSPMedia*, gpointer);

int main (int argc, char *argv[]) {
  GMainLoop *loop;
  GstRTSPServer *server;
  GstRTSPMountPoints *mounts;
  ScarecrowMediaFactory *factory;
  GstRTSPMediaFactory *factory_parent;
  ScarecrowTarget *target;

  gst_init (&argc, &argv);

  server = gst_rtsp_server_new (); /* create a server instance */
  mounts = gst_rtsp_server_get_mount_points (server); /* get the mount points for this server */
  factory = g_object_new(SCARECROW_TYPE_MEDIA_FACTORY, NULL);
  factory_parent = (GstRTSPMediaFactory*)factory;
  gst_rtsp_media_factory_set_shared (factory_parent, TRUE);
  target = g_object_new(SCARECROW_TYPE_TARGET, "stale_t", (const guint64)333333333, NULL);
  g_signal_connect(factory, "media-configure", (GCallback) media_configure, (gpointer) target);
  gst_rtsp_mount_points_add_factory (mounts, "/scarecrow", factory_parent); /* attach the test factory to the /scarecrow url */
  g_object_unref (mounts);
  gst_rtsp_server_attach (server, NULL); /* attach the server to the default maincontext */

  /* start serving */
  loop = g_main_loop_new (NULL, FALSE);
  g_print ("stream ready at rtsp://jetsontx1b:8554/scarecrow\n");
  g_main_loop_run (loop);

  return 0;
}

static void media_configure(GstRTSPMediaFactory *factory, GstRTSPMedia *media, gpointer target) {
  GstElement *element, *appsink;
  GstAppSinkCallbacks ascbs = {NULL, NULL, scarecrow_target_update};
  element = gst_rtsp_media_get_element(media);
  /* GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(element),GST_DEBUG_GRAPH_SHOW_CAPS_DETAILS,"/home/joe/sd/code/scarecrow/gstreamer_graphs/scarecrow.dot"); */
  appsink = gst_bin_get_by_name_recurse_up(GST_BIN(element), "targetconnect");
  gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &ascbs, target, NULL);
  gst_object_unref(appsink);
  gst_object_unref(element);
}
