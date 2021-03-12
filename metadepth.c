#include "metadepth.h"

gpointer meta_depth_copy_func(gpointer data, gpointer user_data) {
  MetaDepth *src_meta_depth = (MetaDepth *)data;
  MetaDepth *dst_meta_depth = (MetaDepth *)g_malloc0(sizeof(MetaDepth));
  dst_meta_depth->depth_buf = src_meta_depth->depth_buf;
  if(dst_meta_depth->depth_buf) {
    gst_buffer_ref(dst_meta_depth->depth_buf);
  }
  return (gpointer)dst_meta_depth;
}

void meta_depth_release_func(gpointer data, gpointer user_data) {
  MetaDepth *meta_depth = (MetaDepth *)data;
  if(meta_depth) {
    if(meta_depth->depth_buf) {
      gst_buffer_unref(meta_depth->depth_buf);
      meta_depth->depth_buf = NULL;
    }
    g_free(meta_depth);
    meta_depth = NULL;
  }
}

gpointer meta_depth_gst_to_nvds_transform_func(gpointer data, gpointer user_data) {
  NvDsUserMeta *user_meta = (NvDsUserMeta *)data;
  MetaDepth *src_meta_depth = (MetaDepth *)user_meta->user_meta_data;
  MetaDepth *dst_meta_depth = (MetaDepth *)meta_depth_copy_func(src_meta_depth, NULL);
  return (gpointer)dst_meta_depth;
}

void meta_depth_gst_to_nvds_release_func(gpointer data, gpointer user_data) {
  NvDsUserMeta *user_meta = (NvDsUserMeta*)data;
  MetaDepth *meta_depth = (MetaDepth*)user_meta->user_meta_data;
  meta_depth_release_func(meta_depth, NULL);
}
