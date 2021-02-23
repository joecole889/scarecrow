#include "scarecrow-media-factory.h"

struct _ScarecrowMediaFactory {
	GstRTSPMediaFactory parent;
};

G_DEFINE_TYPE (ScarecrowMediaFactory, scarecrow_media_factory, GST_TYPE_RTSP_MEDIA_FACTORY);

static void scarecrow_media_factory_class_init(ScarecrowMediaFactoryClass *klass) {
	GstRTSPMediaFactoryClass *parent_class = GST_RTSP_MEDIA_FACTORY_CLASS(klass);
	parent_class->create_element = scarecrow_create_element;
}

static void scarecrow_media_factory_init(ScarecrowMediaFactory *self) {
}

GstElement *
scarecrow_create_element (GstRTSPMediaFactory * factory, const GstRTSPUrl * url)
{
  GstElement *element;
  GError *error = NULL;

  /* build the gstreamer bin element with all needed components */
  GstElement *zedsrc = NULL,
             *zedsplit = NULL,
	           *filter1 = NULL,
	           *converter1 = NULL,
	           *filter2 = NULL,
	           *converter2 = NULL,
	           *filter3 = NULL,
	           *mux = NULL,
	           *inference = NULL,
	           *demux = NULL,
	           *tracker = NULL,
	           *filter4 = NULL,
             *tee = NULL,
	           *queue1 = NULL,
	           *osd = NULL,
	           *converter3 = NULL,
             *filter5 = NULL,
	           *encoder = NULL,
	           *rtppay = NULL,
             *appsink = NULL;

  zedsrc = gst_element_factory_make("zedsrc","zed-source");
  zedsplit = gst_element_factory_make("zedsplit","zed-split");
  filter1 = gst_element_factory_make("capsfilter","BGRA");
  converter1 = gst_element_factory_make("videoconvert","vidconv1");
  filter2 = gst_element_factory_make("capsfilter","RGBA");
  converter2 = gst_element_factory_make("nvvideoconvert","vidconv2");
  filter3 = gst_element_factory_make("capsfilter","nvmemcopy");
  mux = gst_element_factory_make("nvstreammux","mux");
  inference = gst_element_factory_make("nvinfer","infer");
  demux = gst_element_factory_make("nvstreamdemux","demux");
  tracker = gst_element_factory_make("nvtracker","tracker");
  filter4 = gst_element_factory_make("capsfilter","noop");
  tee = gst_element_factory_make("tee","t");
  queue1 = gst_element_factory_make("queue","q");
  osd = gst_element_factory_make("nvdsosd","osd");
  converter3 = gst_element_factory_make("nvvideoconvert","vidconv3");
  filter5 = gst_element_factory_make("capsfilter","NV12");
  encoder = gst_element_factory_make("nvv4l2h264enc","encoder");
  rtppay = gst_element_factory_make("rtph264pay","pay0");
  appsink = gst_element_factory_make("appsink","targetconnect");
  if (!zedsrc || ! zedsplit || !filter1 || !converter1 || !filter2 || !converter2 || !filter3 ||
      !mux || !inference || !tracker || !demux ||
      !filter4 || !tee || !queue1 || !osd || !converter3 || !filter5 || !encoder || !rtppay ||
      !appsink) {
    error = g_error_new_literal(0,1,"Problem creating the gstreamer elements.");
    goto build_error;
  }

  g_object_set(G_OBJECT(zedsrc),
               "resolution", 2,
               "framerate", 15,
               "stream-type", 4,
               "enable-positional-tracking", 0,
               "camera-is-static", 1, NULL);
  g_object_set(G_OBJECT(filter1),
               "caps", gst_caps_from_string("video/x-raw,format=BGRA"), NULL);
  g_object_set(G_OBJECT(filter2),
               "caps", gst_caps_from_string("video/x-raw,format=RGBA"), NULL);
  g_object_set(G_OBJECT(filter3),
               "caps", gst_caps_from_string("video/x-raw(memory:NVMM),format=RGBA"), NULL);
  g_object_set(G_OBJECT(mux),
               "batch-size", 5,
               "width", 1280,
               "height", 720,
               "batched-push-timeout", 120000, NULL);
  g_object_set(G_OBJECT(inference),
               "batch-size", 5,
               "config-file-path", "/home/joe/sd/code/scarecrow_eyes/zed_peoplenet.txt", NULL);
  g_object_set(G_OBJECT(tracker),
               "tracker-width", 1280,
               "tracker-height", 720,
               "ll-lib-file", "/opt/nvidia/deepstream/deepstream/lib/libnvds_mot_klt.so",
               "enable-batch-process", 1, NULL);
  g_object_set(G_OBJECT(filter4),
               "caps", gst_caps_from_string("video/x-raw(memory:NVMM),format=RGBA"), NULL);
  g_object_set(G_OBJECT(osd),
               "process-mode", 2, NULL);
  g_object_set(G_OBJECT(filter5),
               "caps", gst_caps_from_string("video/x-raw(memory:NVMM),format=NV12,width=1280,height=720"),
               NULL);
  g_object_set(G_OBJECT(rtppay),
               "pt", 96, NULL);
  g_object_set(G_OBJECT(appsink),
               "sync", FALSE,
               "max-buffers", 5,
               "drop", TRUE,
               "wait-on-eos", FALSE, NULL);

  element = gst_bin_new("scarecrow-bin");

  gst_bin_add_many(GST_BIN(element),
		   zedsrc,zedsplit,filter1,converter1,filter2,converter2,filter3,
		   mux,inference,demux,tracker,
		   filter4,tee,queue1,osd,converter3,filter5,encoder,rtppay,
       appsink,NULL);

  if (!gst_element_link_many(zedsrc,zedsplit,filter1,converter1,filter2,converter2,filter3,NULL) ||
      !gst_element_link_many(mux,inference,demux,NULL) ||
      !gst_element_link_many(tracker,filter4,tee,NULL) ||
      !gst_element_link_many(queue1,osd,converter3,filter5,encoder,rtppay,NULL)) {
    error = g_error_new_literal(0,1,"Problem in linking static element pads.");
    goto build_error;
  }

  GstPad *sinkpad=NULL, *srcpad=NULL;
  sinkpad = gst_element_get_request_pad(mux,"sink_0");
  srcpad = gst_element_get_static_pad(filter3,"src");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    error = g_error_new_literal(0,1,"Problem in linking filter3 to mux.");
    goto build_error;
  }
  gst_object_unref(sinkpad); sinkpad = NULL;
  gst_object_unref(srcpad); srcpad = NULL;

  sinkpad = gst_element_get_static_pad(tracker,"sink");
  srcpad = gst_element_get_request_pad(demux,"src_0");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    error = g_error_new_literal(0,1,"Problem in linking demux to tracker.");
    goto build_error;
  }
  gst_object_unref(sinkpad); sinkpad = NULL;
  gst_object_unref(srcpad); srcpad = NULL;

  sinkpad = gst_element_get_static_pad(queue1,"sink");
  srcpad = gst_element_get_request_pad(tee,"src_0");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    error = g_error_new_literal(0,1,"Problem in linking tee to queue1.");
    goto build_error;
  }
  gst_object_unref(sinkpad); sinkpad = NULL;
  gst_object_unref(srcpad); srcpad = NULL;

  sinkpad = gst_element_get_static_pad(appsink,"sink");
  srcpad = gst_element_get_request_pad(tee,"src_1");
  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    error = g_error_new_literal(0,1,"Problem in linking tee to appsink.");
    goto build_error;
  }
  gst_object_unref(sinkpad); sinkpad = NULL;
  gst_object_unref(srcpad); srcpad = NULL;

  if (element == NULL)
    goto build_error;

  if (error != NULL) { /* a recoverable error was encountered */
    GST_WARNING ("recoverable parsing error: %s", error->message);
    g_error_free (error);
  }
  return element;

/* ERRORS */
build_error:
  {
    g_critical("could not build the gstreamer bin: %s", 
               (error ? error->message : "unknown reason"));
    if (error)
      g_error_free (error);
    if (sinkpad) gst_object_unref(sinkpad);
    if (srcpad) gst_object_unref(srcpad);
    return NULL;
  }
}
