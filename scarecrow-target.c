#include "scarecrow-target.h"

#define SCARECROW_TARGET_FILTER_SIZE 3
#define SCARECROW_TARGET_FILTER_LENGTH 9
const float scarecrow_target_avg_filter[] = {0.0625, 0.125, 0.0625, 0.125, 0.25, 0.125, 0.0625, 0.125, 0.0625};

#define SCARECROW_TARGET_LOG_DOMAIN ("scarecrow-target")

struct _ScarecrowTarget {
	GObject parent_instance;
	guint64 id;
	guint  x;
	guint  y;
	guint depth;
	struct timespec t;
  guint64 stale_t;
  guint class_id;
  gfloat confidence;
  gfloat conf_thresh;
  guint frame_width;
  guint frame_height;
  enum state mode;
  pthread_mutex_t lock;
};

G_DEFINE_TYPE (ScarecrowTarget,scarecrow_target,G_TYPE_OBJECT)

enum {
	PROP_ID = 1,
	PROP_X,
	PROP_Y,
	PROP_DEPTH,
  PROP_STALE_T,
  PROP_CLASS_ID,
  PROP_CONF_THRESH,
  PROP_FRAME_WIDTH,
  PROP_FRAME_HEIGHT,
	N_PROPS
};

static void scarecrow_target_finalize(GObject *gobj) {
  ScarecrowTarget *self = (ScarecrowTarget *)gobj;
  pthread_mutex_destroy(&(self->lock));
  G_OBJECT_CLASS(scarecrow_target_parent_class)->finalize(gobj);
}

static GParamSpec *obj_properties[N_PROPS] = {NULL,};

static void scarecrow_target_get_property(GObject *object,
		                                      guint prop_id,
																					GValue *value,
																					GParamSpec *pspec)
{
	ScarecrowTarget *self = (ScarecrowTarget *)object;

	switch (prop_id) {
		case PROP_ID:
			g_value_set_uint64(value, scarecrow_target_get_id(self));
			break;
		case PROP_X:
			g_value_set_uint(value, scarecrow_target_get_x(self));
			break;
		case PROP_Y:
			g_value_set_uint(value, scarecrow_target_get_y(self));
			break;
		case PROP_DEPTH:
			g_value_set_uint(value, scarecrow_target_get_depth(self));
			break;
		case PROP_STALE_T:
			g_value_set_uint64(value, scarecrow_target_get_stale_t(self));
			break;
		case PROP_CLASS_ID:
			g_value_set_uint(value, scarecrow_target_get_class_id(self));
			break;
		case PROP_CONF_THRESH:
			g_value_set_float(value, scarecrow_target_get_conf_thresh(self));
			break;
		case PROP_FRAME_WIDTH:
			g_value_set_uint(value, scarecrow_target_get_frame_width(self));
			break;
		case PROP_FRAME_HEIGHT:
			g_value_set_uint(value, scarecrow_target_get_frame_height(self));
			break;
		default:
		  G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
	}
}

static void scarecrow_target_set_property(GObject *object,
		                                      guint prop_id,
																					const GValue *value,
																					GParamSpec *pspec)
{
	ScarecrowTarget *self = (ScarecrowTarget *)object;

	switch (prop_id) {
		case PROP_ID:
		  scarecrow_target_set_id(self,g_value_get_uint64(value));
			break;
		case PROP_X:
		  scarecrow_target_set_x(self,g_value_get_uint(value));
			break;
		case PROP_Y:
		  scarecrow_target_set_y(self,g_value_get_uint(value));
			break;
		case PROP_DEPTH:
		  scarecrow_target_set_depth(self,g_value_get_uint(value));
			break;
		case PROP_STALE_T:
		  scarecrow_target_set_stale_t(self,g_value_get_uint64(value));
			break;
		case PROP_CLASS_ID:
		  scarecrow_target_set_class_id(self,g_value_get_uint(value));
			break;
		case PROP_CONF_THRESH:
		  scarecrow_target_set_conf_thresh(self,g_value_get_float(value));
			break;
		case PROP_FRAME_WIDTH:
		  scarecrow_target_set_frame_width(self,g_value_get_uint(value));
			break;
		case PROP_FRAME_HEIGHT:
		  scarecrow_target_set_frame_height(self,g_value_get_uint(value));
			break;
		default:
		  G_OBJECT_WARN_INVALID_PROPERTY_ID(object,prop_id,pspec);
	}
}

static void scarecrow_target_class_init(ScarecrowTargetClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = scarecrow_target_get_property;
	object_class->set_property = scarecrow_target_set_property;
  object_class->finalize = scarecrow_target_finalize;

	obj_properties[PROP_ID] = g_param_spec_uint64("id",
			                                          "Id",
																							  "Deepstream assigned target identification number",
																							  0,
																							  0xFFFFFFFFFFFFFFFF,
																							  0xFFFFFFFFFFFFFFFF,
																							  G_PARAM_READWRITE);
	obj_properties[PROP_X] = g_param_spec_uint("x",
			                                       "x",
																						 "Target x location",
																						 0,
																						 65535,
																						 0,
																						 G_PARAM_READWRITE);
	obj_properties[PROP_Y] = g_param_spec_uint("y",
			                                       "y",
																						 "Target y location",
																						 0,
																						 65535,
																						 0,
																						 G_PARAM_READWRITE);
	obj_properties[PROP_DEPTH] = g_param_spec_uint("depth",
			                                           "Depth",
																							   "Distance from the robot camera to the target",
																							   0,
																							   65535,
																							   0,
																							   G_PARAM_READWRITE);
	obj_properties[PROP_STALE_T] = g_param_spec_uint64("stale_t",
      			                                         "stale_t",
      																						   "Allowed ellapsed time before a target becomes stale",
      																						   0,
      																							 0xFFFFFFFFFFFFFFFF,
      																						   1000000000,
      																						   (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));
	obj_properties[PROP_CLASS_ID] = g_param_spec_uint("class_id",
			                                              "class_id",
																							      "ID number of the class of objects to target",
																							      0,
																							      65535,
																							      0,
      																						 (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));
	obj_properties[PROP_CONF_THRESH] = g_param_spec_float("conf_thresh",
			                                                  "conf_thresh",
																						            "Confidence threshold necessary to accept a detected target",
																						            0.0f,
																							          1.0f,
																						            0.5f,
																						            (G_PARAM_CONSTRUCT | G_PARAM_READWRITE));
	obj_properties[PROP_FRAME_WIDTH] = g_param_spec_uint("frame_width",
			                                                 "frame_width",
																							         "Width of the video frame in which the current target was detected",
																							         0,
																							         65535,
																							         0,
																						           G_PARAM_READWRITE);
	obj_properties[PROP_FRAME_HEIGHT] = g_param_spec_uint("frame_height",
			                                                  "frame_height",
																							          "Height of the video frame in which the current target was detected",
																							          0,
																							          65535,
																							          0,
																						            G_PARAM_READWRITE);

	g_object_class_install_properties(object_class,N_PROPS,obj_properties);
}

static void scarecrow_target_init(ScarecrowTarget *self) {
  pthread_mutex_init(&(self->lock),NULL);
  self->mode = SEEKING;
  self->confidence = 0.0f;
  clock_gettime(CLOCK_MONOTONIC, &(self->t));
}

const guint64 scarecrow_target_get_id(ScarecrowTarget *self) {
	return self->id;
}

void scarecrow_target_set_id(ScarecrowTarget *self, const guint64 id) {
	self->id = id;
}

const guint scarecrow_target_get_x(ScarecrowTarget *self) {
	return self->x;
}

void scarecrow_target_set_x(ScarecrowTarget *self, const guint x) {
	self->x = x;
}

const guint scarecrow_target_get_y(ScarecrowTarget *self) {
	return self->y;
}

void scarecrow_target_set_y(ScarecrowTarget *self, const guint y) {
	self->y = y;
}

const guint scarecrow_target_get_depth(ScarecrowTarget *self) {
	return self->depth;
}

void scarecrow_target_set_depth(ScarecrowTarget *self, const guint depth) {
	self->depth = depth;
}

const guint64 scarecrow_target_get_stale_t(ScarecrowTarget *self) {
  return self->stale_t;
}

void scarecrow_target_set_stale_t(ScarecrowTarget *self, const guint64 stale_t) {
	self->stale_t = stale_t;
}

const guint scarecrow_target_get_class_id(ScarecrowTarget *self) {
	return self->class_id;
}

void scarecrow_target_set_class_id(ScarecrowTarget *self, const guint class_id) {
	self->class_id = class_id;
}

const gfloat scarecrow_target_get_conf_thresh(ScarecrowTarget *self) {
	return self->conf_thresh;
}

void scarecrow_target_set_conf_thresh(ScarecrowTarget *self, const gfloat conf_thresh) {
	self->conf_thresh = conf_thresh;
}

const guint scarecrow_target_get_frame_width(ScarecrowTarget *self) {
	return self->frame_width;
}

void scarecrow_target_set_frame_width(ScarecrowTarget *self, const guint frame_width) {
	self->frame_width = frame_width;
}

const guint scarecrow_target_get_frame_height(ScarecrowTarget *self) {
	return self->frame_height;
}

void scarecrow_target_set_frame_height(ScarecrowTarget *self, const guint frame_height) {
	self->frame_height = frame_height;
}

GstFlowReturn scarecrow_target_update(GstAppSink *appsink, gpointer self_ptr) {
  gchar message[120];
  guint ii, jj, ind, row, col, start_row, start_col;
  guint center_x, center_y, depth;
  float depth_f;
  struct timespec frame_time_in, frame_time_out;
  guint64 t_now;
  GstMeta *gst_meta;
  NvDsMeta *nvdsmeta;
  NvDsMetaList *l_user_meta = NULL;
  NvDsUserMeta *user_meta = NULL;
  NvDsBatchMeta *batch_meta;
  NvDsFrameMeta *frame_meta;
  GstBuffer* depth_buf = NULL;
  GstMapInfo minfo;
  guint16 *depth_data = NULL;
  ScarecrowTarget *self = (ScarecrowTarget*)self_ptr;

  GstSample *sample = gst_app_sink_pull_sample(appsink);
  if(!sample) return GST_FLOW_FLUSHING;
  GstBuffer *buf = gst_sample_get_buffer(sample);
  gst_sample_unref(sample);

  batch_meta = gst_buffer_get_nvds_batch_meta(buf);
  if (batch_meta == NULL) {
    g_print("Nvidia Deepstream batch meta data not found at appsink.\n");
    return GST_FLOW_ERROR;
  }

  for(ii=0; ii<batch_meta->num_frames_in_batch; ii++) {
    frame_meta = nvds_get_nth_frame_meta(batch_meta->frame_meta_list, ii);

    for(l_user_meta=frame_meta->frame_user_meta_list; l_user_meta!=NULL; l_user_meta=l_user_meta->next) {
      user_meta = (NvDsUserMeta*)(l_user_meta->data);
      if(user_meta->base_meta.meta_type == NVDS_GST_DEPTH_META) {
        depth_buf = gst_buffer_ref(((MetaDepth*)user_meta->user_meta_data)->depth_buf);
        break;
      }
    }
    if(!gst_buffer_map(depth_buf, &minfo, GST_MAP_READ)) {
      if(depth_buf) gst_buffer_unref(depth_buf);
      return GST_FLOW_ERROR;
    }
    depth_data = (guint16*)minfo.data;

    clock_gettime(CLOCK_MONOTONIC, &(frame_time_in));
    timespec_subtract(&frame_time_out, &frame_time_in, &self->t);
    t_now = timespec_int(&(frame_time_out));
    if(t_now > self->stale_t) {
      g_snprintf(message,120,"Target grew stale (elapsed time = %lu > %lu).",
        t_now, self->stale_t);
      g_log_default_handler(SCARECROW_TARGET_LOG_DOMAIN,G_LOG_LEVEL_DEBUG,message,NULL);
      pthread_mutex_lock(&(self->lock));
      self->mode = SEEKING;
      pthread_mutex_unlock(&(self->lock));
    }

    NvDsFrameMetaList *fmeta_list = NULL;
    NvDsObjectMeta *obj_meta = NULL;
    for(fmeta_list = frame_meta->obj_meta_list; fmeta_list != NULL; fmeta_list = fmeta_list->next) {
      obj_meta = (NvDsObjectMeta *)fmeta_list->data;
      if(obj_meta &&
         obj_meta->class_id == self->class_id &&
         obj_meta->confidence >= self->conf_thresh)
      {
        if(obj_meta->object_id == self->id || self->mode == SEEKING)
        {
          center_y = obj_meta->tracker_bbox_info.org_bbox_coords.top +
                     obj_meta->tracker_bbox_info.org_bbox_coords.height/2;
          center_x = obj_meta->tracker_bbox_info.org_bbox_coords.left +
                     obj_meta->tracker_bbox_info.org_bbox_coords.width/2;
          if(obj_meta->tracker_bbox_info.org_bbox_coords.width >= SCARECROW_TARGET_FILTER_SIZE &&
             obj_meta->tracker_bbox_info.org_bbox_coords.height >= SCARECROW_TARGET_FILTER_SIZE)
          {
            depth_f = 0.0f;
            start_row = obj_meta->tracker_bbox_info.org_bbox_coords.top;
            start_col = obj_meta->tracker_bbox_info.org_bbox_coords.left;
            for(jj=0; jj<SCARECROW_TARGET_FILTER_LENGTH; jj++) {
              row = start_row + jj/SCARECROW_TARGET_FILTER_SIZE;
              col = start_col + jj%SCARECROW_TARGET_FILTER_SIZE;
              ind = row * frame_meta->source_frame_width + col;
              depth_f += scarecrow_target_avg_filter[jj] * (float)(*(depth_data + ind));
            }
            depth = (guint)roundf(depth_f);
          } else {
            ind = center_y * frame_meta->source_frame_width + center_x;
            depth = *(depth_data + ind);
          }
          
          pthread_mutex_lock(&(self->lock));
          self->id = obj_meta->object_id;
          self->x = center_x;
          self->y = center_y;
          self->depth = depth;
          self->t.tv_sec = frame_time_in.tv_sec;
          self->t.tv_nsec = frame_time_in.tv_nsec;
          self->frame_width = frame_meta->source_frame_width;
          self->frame_height = frame_meta->source_frame_height;
          self->mode = ACQUIRED;
          self->confidence = obj_meta->confidence;
          pthread_mutex_unlock(&(self->lock));

          break;
        }
      }
    }

    if(self->mode == ACQUIRED) {
      g_snprintf(message,120,"Frame #%u, tracking target #%lu, at (%u, %u, %u), confidence = %.3f",
        frame_meta->frame_num,self->id,self->x,self->y,self->depth,self->confidence);
      g_log_default_handler(SCARECROW_TARGET_LOG_DOMAIN,G_LOG_LEVEL_DEBUG,message,NULL);
    } else {
      g_snprintf(message,120,"Seeking a target.");
      g_log_default_handler(SCARECROW_TARGET_LOG_DOMAIN,G_LOG_LEVEL_DEBUG,message,NULL);
    }

    gst_buffer_unref(depth_buf);
    depth_buf = NULL;
  }

  return GST_FLOW_OK;
}

void scarecrow_target_get_location(ScarecrowTarget *self, unsigned char *x, unsigned char *y, unsigned short int *depth) {
  pthread_mutex_lock(&(self->lock));
  x[0] = (unsigned char)(self->x & 0xff);
  x[1] = (unsigned char)((self->x >> 8) & 0xff);
  y[0] = (unsigned char)(self->y & 0xff);
  y[1] = (unsigned char)((self->y >> 8) & 0xff);
  *depth = (unsigned short int)self->depth;
  pthread_mutex_unlock(&(self->lock));
}

int timespec_subtract (struct timespec *result, struct timespec *x, struct timespec *y) {
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_nsec < y->tv_nsec) {
    int nsec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
    y->tv_nsec -= 1000000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_nsec - y->tv_nsec > 1000000000) {
    int nsec = (x->tv_nsec - y->tv_nsec) / 1000000000;
    y->tv_nsec += 1000000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait. tv_nsec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_nsec = x->tv_nsec - y->tv_nsec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

guint64 timespec_int (struct timespec *t) {
  return t->tv_sec * 1000000000 + t->tv_nsec;
}
