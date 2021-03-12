/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2021 Joe Cole <<joe@vernercole.net>>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>

#include "gstzedsplit.h"

GST_DEBUG_CATEGORY_STATIC (gst_zed_split_debug);
#define GST_CAT_DEFAULT gst_zed_split_debug

/* Filter args */
enum
{
  PROP_0,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_MIN_DEPTH_BUF,
  PROP_MAX_DEPTH_BUF
};

/* the capabilities of the inputs and outputs.
 *
 * Takes the output of a StereoLabs Zed camera in BGRA format.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE("BGRA"))
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE("BGRA"))
    );

#define gst_zed_split_parent_class parent_class
G_DEFINE_TYPE (GstZedSplit, gst_zed_split, GST_TYPE_BASE_TRANSFORM);

static void gst_zed_split_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_zed_split_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_zed_split_transform_ip (GstBaseTransform *base, GstBuffer *outbuf);
static gboolean gst_zed_split_setcaps (GstZedSplit*, GstPadDirection, GstCaps*);
static gboolean gst_zed_split_sink_event (GstBaseTransform*, GstEvent*);
static gboolean gst_zed_split_src_event (GstBaseTransform*, GstEvent*);
static GstFlowReturn gst_zed_split_prepare_output_buffer (GstBaseTransform*, GstBuffer*, GstBuffer**);
static gboolean gst_zed_split_propose_allocation (GstBaseTransform*, GstQuery*, GstQuery*);
static GstCaps* gst_zed_split_transform_caps (GstBaseTransform*, GstPadDirection, GstCaps*, GstCaps*);
static gboolean gst_zed_split_get_unit_size(GstBaseTransform*, GstCaps*, gsize*);
static gboolean gst_zed_split_start(GstBaseTransform*);
static gboolean gst_zed_split_stop(GstBaseTransform*);

/* GObject vmethod implementations */

/* initialize the zedsplit's class */
static void
gst_zed_split_class_init (GstZedSplitClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  gobject_class->set_property = gst_zed_split_set_property;
  gobject_class->get_property = gst_zed_split_get_property;

  g_object_class_install_property(gobject_class,
                                  PROP_WIDTH,
                                  g_param_spec_uint("width",
                                                    "width",
                                                    "width of the depth image",
                                                    0,
                                                    1000000000,
                                                    0,
                                                    G_PARAM_CONSTRUCT|G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,
                                  PROP_HEIGHT,
                                  g_param_spec_uint("height",
                                                    "height",
                                                    "height of the depth image",
                                                    0,
                                                    1000000000,
                                                    0,
                                                    G_PARAM_CONSTRUCT|G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,
                                  PROP_MIN_DEPTH_BUF,
                                  g_param_spec_uint("min-depth-bufs",
                                                    "min-depth-bufs",
                                                    "Minimum number of pooled buffers to hold frame depth information",
                                                    5,
                                                    100,
                                                    15,
                                                    G_PARAM_CONSTRUCT|G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,
                                  PROP_MAX_DEPTH_BUF,
                                  g_param_spec_uint("max-depth-bufs",
                                                    "max-depth-bufs",
                                                    "Maximum number of pooled buffers to hold frame depth information",
                                                    5,
                                                    100,
                                                    30,
                                                    G_PARAM_CONSTRUCT|G_PARAM_READWRITE));

  gst_element_class_set_details_simple (gstelement_class,
      "ZedSplit",
      "Generic/Filter",
      "Move StereoLabs Zed camera depth information to metadata", "Joe <<joe@vernercole.net>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  gstbasetransform_class->prepare_output_buffer = GST_DEBUG_FUNCPTR (gst_zed_split_prepare_output_buffer);
  gstbasetransform_class->transform_ip          = GST_DEBUG_FUNCPTR (gst_zed_split_transform_ip);
  gstbasetransform_class->sink_event            = GST_DEBUG_FUNCPTR (gst_zed_split_sink_event);
  gstbasetransform_class->src_event             = GST_DEBUG_FUNCPTR (gst_zed_split_src_event);
  gstbasetransform_class->propose_allocation    = GST_DEBUG_FUNCPTR (gst_zed_split_propose_allocation);
  gstbasetransform_class->transform_caps        = GST_DEBUG_FUNCPTR (gst_zed_split_transform_caps);
  gstbasetransform_class->get_unit_size         = GST_DEBUG_FUNCPTR (gst_zed_split_get_unit_size);
  gstbasetransform_class->start                 = GST_DEBUG_FUNCPTR (gst_zed_split_start);
  gstbasetransform_class->stop                  = GST_DEBUG_FUNCPTR (gst_zed_split_stop);

  /* debug category for filtering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_zed_split_debug, "zedsplit", 0, "element to move Zed camera depth information to metadata");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_zed_split_init (GstZedSplit * filter)
{
  GstBaseTransform *btrans = GST_BASE_TRANSFORM(filter);
  gst_base_transform_set_in_place(btrans,TRUE);
  filter->bufpool = NULL;
}

static void
gst_zed_split_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstZedSplit *filter = GST_ZED_SPLIT (object);

  switch (prop_id) {
    case PROP_WIDTH:
      filter->width = g_value_get_uint(value);
      GST_DEBUG("depth video frame width: %u",filter->width);
      break;
    case PROP_HEIGHT:
      filter->height = g_value_get_uint(value);
      GST_DEBUG("depth video frame height: %u",filter->height);
      break;
    case PROP_MIN_DEPTH_BUF:
      filter->min_depth_bufs = g_value_get_uint(value);
      GST_DEBUG("min # of depth buffers: %u",filter->min_depth_bufs);
      break;
    case PROP_MAX_DEPTH_BUF:
      filter->max_depth_bufs = g_value_get_uint(value);
      GST_DEBUG("max # of depth buffers: %u",filter->max_depth_bufs);
      break;
    default: 
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_zed_split_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstZedSplit *filter = GST_ZED_SPLIT (object);

  switch (prop_id) {
    case PROP_WIDTH:
      g_value_set_uint(value,filter->width);
      GST_DEBUG("getting depth video frame width: %u",filter->width);
      break;
    case PROP_HEIGHT:
      g_value_set_uint(value,filter->height);
      GST_DEBUG("getting depth video frame height: %u",filter->height);
      break;
    case PROP_MIN_DEPTH_BUF:
      g_value_set_uint(value,filter->min_depth_bufs);
      GST_DEBUG("getting min # of depth buffers: %u",filter->min_depth_bufs);
      break;
    case PROP_MAX_DEPTH_BUF:
      g_value_set_uint(value,filter->max_depth_bufs);
      GST_DEBUG("getting max # of depth buffers: %u",filter->max_depth_bufs);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

static GstFlowReturn gst_zed_split_prepare_output_buffer (GstBaseTransform* btrans,
                                                          GstBuffer* inbuf,
                                                          GstBuffer** outbuf) {
  GstFlowReturn ret;
  GST_DEBUG("Calling _prepare_output_buffer()");
  GstZedSplit *self = GST_ZED_SPLIT(btrans);
  GstBufferCopyFlags flags = GST_BUFFER_COPY_FLAGS |
                             GST_BUFFER_COPY_TIMESTAMPS |
                             GST_BUFFER_COPY_META |
                             GST_BUFFER_COPY_MEMORY;
  NvDsMeta *nvds_depth_meta = NULL;
  MetaDepth *meta_depth = NULL;
  GstMapInfo minfo_in, minfo_out;
  if(!gst_buffer_map(inbuf, &minfo_in, GST_MAP_READ)) {
    return GST_FLOW_ERROR;
  }

  /* create the output buffer using shallow copy */
  *outbuf = gst_buffer_copy_region(inbuf, flags, 0, minfo_in.size/2);

  /* prepare a separate GstBuffer to hold depth information as Deepstream meta data */
  meta_depth = (MetaDepth*)g_malloc0(sizeof(MetaDepth));
  ret = gst_buffer_pool_acquire_buffer(self->bufpool, &(meta_depth->depth_buf), NULL);
  if(ret != GST_FLOW_OK) {
    gst_buffer_unmap(inbuf, &minfo_in);
    return ret;
  }
  nvds_depth_meta = gst_buffer_add_nvds_meta(*outbuf,
                                             meta_depth,
                                             NULL,
                                             meta_depth_copy_func,
                                             meta_depth_release_func);
  nvds_depth_meta->meta_type = (GstNvDsMetaType)NVDS_GST_DEPTH_META;
  nvds_depth_meta->gst_to_nvds_meta_transform_func = meta_depth_gst_to_nvds_transform_func;
  nvds_depth_meta->gst_to_nvds_meta_release_func = meta_depth_gst_to_nvds_release_func;

  /* copy the depth information into the prepared buffer */
  if(!gst_buffer_map(meta_depth->depth_buf, &minfo_out, GST_MAP_WRITE)) {
     gst_buffer_unmap(inbuf, &minfo_in);
    return GST_FLOW_ERROR;
  }
  guint32* depth_data_in = (guint32*)(minfo_in.data + minfo_in.size/2);
  guint16* depth_data_out = (guint16*)(minfo_out.data);
  for(unsigned long ii=0; ii<minfo_out.size/(sizeof(guint16)); ii++) {
    *(depth_data_out++) = (guint16)(*(depth_data_in++));
  }
  gst_buffer_unmap(meta_depth->depth_buf, &minfo_out);
  gst_buffer_unmap(inbuf, &minfo_in);

  return GST_FLOW_OK;
}

/* this function does the actual processing */
static GstFlowReturn
gst_zed_split_transform_ip (GstBaseTransform * base, GstBuffer * buf)
{
  GST_DEBUG("Calling _transform_ip()");
  GstZedSplit *filter = GST_ZED_SPLIT (base);

  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (buf)))
    gst_object_sync_values (GST_OBJECT (filter), GST_BUFFER_TIMESTAMP (buf));

  GST_DEBUG("Transforming input buffer to output (in-place).\n");

  return GST_FLOW_OK;
}

static gboolean
gst_zed_split_get_unit_size(GstBaseTransform *trans,
                            GstCaps *caps,
                            gsize *size) {
  GST_DEBUG("Calling _get_unit_size()");
  gboolean ret = TRUE;
  GstZedSplit *self = GST_ZED_SPLIT(trans);
  if(self->height && self->width) {
    *size = 4 * self->height * self->width;
  } else {
    *size = 0;
    ret = FALSE;
  }
  GST_DEBUG("get_unit_size() reporting size of %u",(unsigned int)*size);

  return ret;
}

static GstCaps* gst_zed_split_transform_caps (GstBaseTransform* btrans,
                                              GstPadDirection direction,
                                              GstCaps* caps,
                                              GstCaps* filter) {
  GST_DEBUG("Calling _transform_caps()");
  GstCaps *ret, *tempcaps;
  GstStructure *structure, *structure_cp;
  int height;
  guint ncaps;
  unsigned int ii;

  GST_DEBUG_OBJECT (btrans, "transform from: %" GST_PTR_FORMAT, caps);
  GST_DEBUG("transform direction: %u",(unsigned int)direction);

  tempcaps = gst_caps_new_empty();
  ncaps = gst_caps_get_size(caps);
  for(ii=0; ii<ncaps; ii++) {
    structure = gst_caps_get_structure(caps, ii);
    structure_cp = gst_structure_copy(structure);
    if(gst_structure_get_int(structure, "height", &height)) {
      switch(direction) {
        case GST_PAD_SRC:
          gst_structure_set(structure_cp, "height", G_TYPE_INT, height*2, NULL);
          break;
        case GST_PAD_SINK:
          gst_structure_set(structure_cp, "height", G_TYPE_INT, height/2, NULL);
          break;
        default:
          GST_ELEMENT_ERROR (btrans, CORE, CAPS,
               ("Undefined pad direction passed to _transform_caps()"), (NULL));
          return NULL;
      }
    }
    gst_caps_append_structure(tempcaps,structure_cp);
  }

  if (filter) {
    ret = gst_caps_intersect_full(filter, tempcaps, GST_CAPS_INTERSECT_FIRST);
  } else {
    ret = gst_caps_ref(tempcaps);
  }
  gst_caps_unref(tempcaps);

  GST_DEBUG_OBJECT (btrans, "transform to: %" GST_PTR_FORMAT, ret);
  return ret;
}

static gboolean gst_zed_split_setcaps (GstZedSplit *self,
                                       GstPadDirection direction,
                                       GstCaps *caps) {
  GST_DEBUG("Calling _setcaps()");
  GstBaseTransform* trans = GST_BASE_TRANSFORM(self);
  GstBaseTransformClass* trans_class = GST_BASE_TRANSFORM_GET_CLASS(trans);
  GstStructure *structure, *structure_cp;
  int height_in, height_out, width;
  gsize size;
  GstStructure *pool_config = NULL;
  gboolean ret = TRUE;
  GstCaps *othercaps;
  GstPad *otherpad;

  structure = gst_caps_get_structure(caps, 0);
  ret = ret && gst_structure_get_int(structure, "height", &height_in);
  ret = ret && gst_structure_get_int(structure, "width", &width);
  switch(direction) {
    case GST_PAD_SRC:
      height_out = height_in/2;
      otherpad = trans->srcpad;
      break;
    case GST_PAD_SINK:
      height_out = height_in*2;
      otherpad = trans->sinkpad;
      break;
    default:
      GST_ELEMENT_ERROR (self, CORE, CAPS, ("Undefined pad direction passed to _setcaps()"), (NULL));
      return FALSE;
  }
  structure_cp = gst_structure_copy(structure);
  gst_structure_set(structure_cp, "height", G_TYPE_INT, height_out, NULL);
  othercaps = gst_caps_new_full(structure_cp, NULL);
  ret = ret && gst_pad_set_caps(otherpad, othercaps);
  otherpad = NULL;

  if (!ret) {
    GST_ELEMENT_ERROR (self, CORE, CAPS, ("Problem setting caps on other pad."), (NULL));
  }

  if (gst_caps_is_fixed(caps) &&
      gst_caps_is_fixed(othercaps) &&
      !gst_buffer_pool_is_active(self->bufpool))
  {
    if(self->width==0) self->width = width;
    if(self->height==0) self->height = abs(height_in-height_out);
    ret = ret && trans_class->get_unit_size(trans,NULL,&size);
    pool_config = gst_buffer_pool_get_config(self->bufpool);
    gst_buffer_pool_config_set_params(pool_config,
                                      NULL,
                                      size/2,
                                      self->min_depth_bufs,
                                      self->max_depth_bufs);
    ret = ret && gst_buffer_pool_set_config(self->bufpool, pool_config);
    ret = ret && gst_buffer_pool_set_active(self->bufpool, TRUE);
  }

  gst_caps_unref(othercaps);
  return ret;
}

static gboolean gst_zed_split_sink_event (GstBaseTransform* btrans, GstEvent* evt) {
  GST_DEBUG("Calling _sink_event()");
  gboolean ret = TRUE;
  GstZedSplit *self = GST_ZED_SPLIT(btrans);
  GstCaps *caps;

  switch(GST_EVENT_TYPE(evt)) {
    case GST_EVENT_CAPS:
      gst_event_parse_caps(evt, &caps);
      if(gst_pad_check_reconfigure(btrans->srcpad)) {
        ret = gst_zed_split_setcaps(self, GST_PAD_SRC, caps);
        ret = ret && gst_pad_event_default(btrans->sinkpad, GST_OBJECT(btrans), evt);
      }
      break;
    default:
      ret = gst_pad_event_default(btrans->sinkpad, GST_OBJECT(btrans), evt);
  }
  return ret;
}

static gboolean gst_zed_split_src_event (GstBaseTransform* btrans, GstEvent* evt) {
  GST_DEBUG("Calling _src_event()");
  gboolean ret = TRUE;
  GstZedSplit *self = GST_ZED_SPLIT(btrans);
  GstCaps *caps;

  switch(GST_EVENT_TYPE(evt)) {
    case GST_EVENT_CAPS:
      gst_event_parse_caps(evt, &caps);
      if(gst_pad_check_reconfigure(btrans->sinkpad)) {
        ret = gst_zed_split_setcaps(self, GST_PAD_SINK, caps);
        ret = ret && gst_pad_event_default(btrans->srcpad, GST_OBJECT(btrans), evt);
      }
      break;
    default:
      ret = gst_pad_event_default(btrans->srcpad, GST_OBJECT(btrans), evt);
  }
  return ret;
}

static gboolean gst_zed_split_propose_allocation (GstBaseTransform * trans,
                                                  GstQuery * decide_query,
                                                  GstQuery * query) {
  GST_DEBUG("Calling _propose_allocation()");
  gboolean ret;
  GType api;

  if (decide_query == NULL) {
    GST_DEBUG_OBJECT (trans, "doing passthrough query");
    ret = gst_pad_peer_query (trans->srcpad, query);
  } else {
    guint i, n_metas;
    /* non-passthrough, copy all metadata, decide_query does not contain the
     * metadata anymore that depends on the buffer memory */
    n_metas = gst_query_get_n_allocation_metas (decide_query);
    for (i = 0; i < n_metas; i++) {
      const GstStructure *params;

      api = gst_query_parse_nth_allocation_meta (decide_query, i, &params);
      GST_DEBUG_OBJECT (trans, "proposing metadata %s", g_type_name (api));
      gst_query_add_allocation_meta (query, api, params);
    }
    gst_query_add_allocation_meta (query, NVDS_GST_DEPTH_META, NULL);
    ret = TRUE;
  }
  return ret;
}

static gboolean gst_zed_split_start(GstBaseTransform *trans) {
  GST_DEBUG("Calling _start()");
  GstZedSplit *self = GST_ZED_SPLIT(trans);
  self->bufpool = gst_buffer_pool_new();
  return (self->bufpool != NULL);
}

static gboolean gst_zed_split_stop(GstBaseTransform *trans) {
  GST_DEBUG("Calling _stop()");
  gboolean ret = TRUE;
  GstZedSplit *self = GST_ZED_SPLIT(trans);
  gst_buffer_pool_set_flushing(self->bufpool,TRUE);
  ret = ret && gst_buffer_pool_set_active(self->bufpool,FALSE);
  g_object_unref(self->bufpool);
  self->bufpool = NULL;
  return ret;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
zedsplit_init (GstPlugin * zedsplit)
{
  GST_DEBUG_CATEGORY_INIT(gst_zed_split_debug, "zedsplit", 0, "zedsplit plugin");
  return gst_element_register(zedsplit, "zedsplit", GST_RANK_PRIMARY, GST_TYPE_ZEDSPLIT);
}

/* gstreamer looks for this structure to register zedsplits */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    zedsplit,
    "element to move Zed camera depth information to metadata",
    zedsplit_init,
    "1.0", "Proprietary", "joe", "joe")
