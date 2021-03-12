#pragma once

#include <pthread.h>
#include <time.h>
#include <math.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "gstnvdsmeta.h"
#include "gstzedsplit.h"

G_BEGIN_DECLS

#define SCARECROW_TYPE_TARGET (scarecrow_target_get_type())

enum state {
  SEEKING,
  ACQUIRED
};

G_DECLARE_FINAL_TYPE(ScarecrowTarget,scarecrow_target,SCARECROW,TARGET,GObject)

const guint64 scarecrow_target_get_id(ScarecrowTarget *self);
void          scarecrow_target_set_id(ScarecrowTarget *self, const guint64 id);
const guint   scarecrow_target_get_x(ScarecrowTarget *self);
void          scarecrow_target_set_x(ScarecrowTarget *self, const guint x);
const guint   scarecrow_target_get_y(ScarecrowTarget *self);
void          scarecrow_target_set_y(ScarecrowTarget *self, const guint y);
const guint   scarecrow_target_get_depth(ScarecrowTarget *self);
void          scarecrow_target_set_depth(ScarecrowTarget *self, const guint depth);
const guint64 scarecrow_target_get_stale_t(ScarecrowTarget *self);
void          scarecrow_target_set_stale_t(ScarecrowTarget *self, const guint64 stale_t);
const guint   scarecrow_target_get_class_id(ScarecrowTarget *self);
void          scarecrow_target_set_class_id(ScarecrowTarget *self, const guint class_id);
const gfloat  scarecrow_target_get_conf_thresh(ScarecrowTarget *self);
void          scarecrow_target_set_conf_thresh(ScarecrowTarget *self, const gfloat conf_thresh);
const guint   scarecrow_target_get_frame_width(ScarecrowTarget *self);
void          scarecrow_target_set_frame_width(ScarecrowTarget *self, const guint frame_width);
const guint   scarecrow_target_get_frame_height(ScarecrowTarget *self);
void          scarecrow_target_set_frame_height(ScarecrowTarget *self, const guint frame_height);

GstFlowReturn scarecrow_target_update(GstAppSink*,gpointer);
void scarecrow_target_get_location(ScarecrowTarget*,unsigned char*,unsigned char*,unsigned short int*);
int timespec_subtract (struct timespec*, struct timespec*, struct timespec*);
guint64 timespec_int (struct timespec*);

G_END_DECLS
