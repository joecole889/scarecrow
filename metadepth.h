#ifndef _META_DEPTH_H_
#define _META_DEPTH_H_

#include <gst/gst.h>
#include "nvdsmeta.h"

typedef struct _MetaDepth {
  GstBuffer *depth_buf;
} MetaDepth;

gpointer meta_depth_copy_func(gpointer, gpointer);
void meta_depth_release_func(gpointer, gpointer);
gpointer meta_depth_gst_to_nvds_transform_func(gpointer, gpointer);
void meta_depth_gst_to_nvds_release_func(gpointer, gpointer);

#endif /* _META_DEPTH_H_ */
