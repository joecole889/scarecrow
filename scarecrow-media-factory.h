#ifndef __SCARECROW_MEDIA_FACTORY_H__
#define __SCARECROW_MEDIA_FACTORY_H__

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-media-factory.h>

#include "scarecrow-target.h"

G_BEGIN_DECLS

#define SCARECROW_TYPE_MEDIA_FACTORY scarecrow_media_factory_get_type ()
G_DECLARE_FINAL_TYPE (ScarecrowMediaFactory, scarecrow_media_factory, SCARECROW, MEDIA_FACTORY, GstRTSPMediaFactory)

/* Method definitions */
ScarecrowMediaFactory *scarecrow_media_factory_new (void);
GstElement* scarecrow_create_element(GstRTSPMediaFactory *factory, const GstRTSPUrl *url);

G_END_DECLS

#endif /* __SCARECROW_MEDIA_FACTORY_H__ */
