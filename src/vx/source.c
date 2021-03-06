/*
Copyright (c) 2006-2013 Hartmut Seichter
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the VideoExtractor Project.
*/

#include "_source.h"
#include "_sink.h"

#include "_globals.h"

#include "_backends/null/null_backend.h"
#include "_backends/v4l2/v4l2_backend.h"
#include "_backends/dshow/dshow_backend.h"

#if defined(HAVE_GSTREAMER)
	#include <_backends/gst010/gst010_backend.h>
#endif

/* only AVFoundation and QTkit need to be conditional as they use
 * a backend written in Objective-C
 */
#if defined(HAVE_AVFOUNDATION)
	#include "_backends/avfoundation/avfoundation_backend.h"
#endif

#if defined(HAVE_QTKIT)
	#include "_backends/qtkit/qtkit_backend.h"
#endif

int
vx_source_ref_init(vx_source *s) {
	if (s) { VX_OBJECT_CAST(s)->refCount = 0; return 0;}
	return -1;
}

int
vx_source_ref(vx_source *s) {
	if (s) { VX_OBJECT_CAST(s)->refCount++; return 0;}
	return -1;
}

int
vx_source_unref(vx_source *s) {
	if (s) {
		VX_OBJECT_CAST(s)->refCount--;

		if (VX_OBJECT_CAST(s)->refCount == 0) {
			free(s);
		}

		return 0;
	}
	return -1;
}

static char _null[] = "null";
static char _gstreamer[] = "gstreamer";
static char _avfoundation[] = "avfoundation";
static char _qtkit[] = "qtkit";
static char _v4l2[] = "v4l2";
static char _dshow[] = "directshow";


const char* vx_source_default()
{

#if defined(HAVE_V4L2)
	return _v4l2;
#elif defined(HAVE_DSHOW)
	return _dshow;
#elif defined(HAVE_GSTREAMER)
	return _gstreamer;
#elif defined(HAVE_AVFOUNDATION)
	return _avfoundation;
#elif defined(HAVE_QTKIT)
	return _qtkit;
#endif
	return _null;
}



void
_vx_source_destroy(vx_object* source)
{
    /* resets all buffers */
    vx_source_enumerate(VX_SOURCE_CAST(source),0,0);
}




void *
vx_source_create(const char *n) {

	vx_source* result = 0;

	if (n == 0) return vx_source_create(vx_source_default());

	if (0 == strcmp(_null,n))
		result = vx_source_null_create();

	if (0 == strcmp(_v4l2,n))
		result = vx_source_v4l2_create();

	if (0 == strcmp(_dshow,n))
		result = vx_source_dshow_create();

#if defined(HAVE_GSTREAMER)
	if (0 == strcmp(_gstreamer,n)) {
		result = vx_source_gstreamer_create();
	}
#endif

#if defined(HAVE_AVFOUNDATION)
	if (0 == strcmp(_avfoundation,n)) {
		result = vx_source_avfoundation_create();
	}
#endif

#if defined(HAVE_QTKIT)
	if (0 == strcmp(_qtkit,n)) {
		result = vx_source_qtkit_create();
	}
#endif

	vx_source_ref_init(result);

    result->_object.destroy = _vx_source_destroy;

	 return result;
}



int
vx_source_enumerate(vx_source *s, vx_device_description **devices, int *size)
{
	int res = 0;

	/* internal reset */
	if (size == 0L) {

        int i = 0;
        for (i = 0;i < s->deviceCount;++i) {
            free(s->devices[i].capabilities);
            s->devices[i].capabilitiesCount = 0;
        }

		free(s->devices);
		s->deviceCount = 0;
		return 0;
	}

	res = VX_SOURCE_CAST(s)->enumerate(s);

	*devices = s->devices;
	*size = s->deviceCount;

	return res;
}

int
vx_source_open(vx_source *s, const char* uuid,vx_device_capability* cap)
{
	s->sinkCount = 0;
	s->sinks = 0;

    return VX_SOURCE_CAST(s)->open(s,uuid,cap);
}

int
vx_source_close(vx_source *s)
{
	return VX_SOURCE_CAST(s)->close(s);
}

int
vx_source_set_state(vx_source *s, int newstate)
{
	return VX_SOURCE_CAST(s)->set_state(s,newstate);
}

int
vx_source_get_state(vx_source *s, int* state)
{
	return VX_SOURCE_CAST(s)->get_state(s,state);
}

int
vx_source_update(vx_source *s, unsigned int runloop)
{
	return VX_SOURCE_CAST(s)->update(s,runloop);
}

int
vx_source_add_sink(vx_source* source, vx_sink* sink)
{
	int newSize = source->sinkCount+1;

	vx_sink** moreSink = realloc(source->sinks,newSize*sizeof(struct vx_sink*));

	if (moreSink)
	{
		moreSink[source->sinkCount] = sink;

		source->sinks = moreSink;
		source->sinkCount = newSize;

		return 0;
	}

	return -1;
}




/**
 * @brief _vx_source_v4l2_addcapability
 * @param newCap
 * @param caps
 * @param capcount
 * @return
 */
int
_vx_source_addcapability(const vx_device_capability* newCap,vx_device_capability** caps,unsigned int *capcount)
{
    vx_device_capability* oldCaps = *caps;
    unsigned int oldCapCount = *capcount;
    vx_device_capability* newCaps = 0;

    if (oldCapCount == 0) {

        newCaps = malloc(sizeof(struct vx_device_capability));

    } else {

        newCaps = realloc(oldCaps,sizeof(struct vx_device_capability)*(oldCapCount+1));

    }

    if (newCaps) {

        memcpy(&newCaps[oldCapCount],newCap,sizeof(struct vx_device_capability));

        *capcount = oldCapCount+1;
        *caps = newCaps;

        return 0;
    }

    return -1;

}

int
_vx_send_frame(vx_source* source,const vx_frame* frame)
{
	int i = 0;
	for(i = 0;i < source->sinkCount;++i) {

		struct vx_sink* sink = (*source).sinks[i];

        switch (sink->sinkType) {

        case VX_SINK_TYPE_DIRECT:
            sink->frameCallback(source,sink,frame,sink->frameCallbackUserData);
            break;
        case VX_SINK_TYPE_BUFFERED:
            sink->copyCallback(source,sink,frame);
            break;
        case VX_SINK_TYPE_CONVERTED:
            sink->conversionCallback(source,sink,frame);
            break;
        default:
            break;
        }
	}

	return 0;
}

int
_vx_broadcast(vx_source* source)
{
	int i = 0;
	for(i = 0;i < source->sinkCount;++i)
	{
		vx_sink* sink = source->sinks[i];

		if (sink->sinkType == VX_SINK_TYPE_DIRECT) continue;

		if (sink->frameCallback)
			sink->frameCallback(source,sink,&sink->buffer,sink->frameCallbackUserData);
	}

	return 0;
}
