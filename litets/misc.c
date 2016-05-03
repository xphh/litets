#include "litets.h"
#include <string.h>
#include <malloc.h>

int lts_is_video(int type)
{
	switch (type)
	{
	case STREAM_TYPE_VIDEO_MPEG1:
	case STREAM_TYPE_VIDEO_MPEG2:
	case STREAM_TYPE_VIDEO_MPEG4:
	case STREAM_TYPE_VIDEO_H264:
	case STREAM_TYPE_VIDEO_HEVC:
	case STREAM_TYPE_VIDEO_CAVS:
	case STREAM_TYPE_VIDEO_VC1:
	case STREAM_TYPE_VIDEO_DIRAC:
		return 1;
	default:
		return 0;
	}
}

int lts_is_audio(int type)
{
	switch (type)
	{
	case STREAM_TYPE_AUDIO_MPEG1:
	case STREAM_TYPE_AUDIO_MPEG2:
	case STREAM_TYPE_AUDIO_AAC:
	case STREAM_TYPE_AUDIO_AAC_LATM:
	case STREAM_TYPE_AUDIO_AC3:
	case STREAM_TYPE_AUDIO_DTS:
	case STREAM_TYPE_AUDIO_TRUEHD:
		return 1;
	default:
		return 0;
	}
}

#define DEFAULT_BUF_SIZE	(256<<10)

int lts_buffer_handle(TBufferHandler *handler)
{
	uint8_t *buf;
	int buf_len = 0;

	if (!handler)
	{
		return -1;
	}
	if (!handler->input || !handler->output)
	{
		return -1;
	}
	if (handler->buf_size <= 0)
	{
		handler->buf_size = DEFAULT_BUF_SIZE;
	}

	buf = (uint8_t *)malloc(handler->buf_size);

	while (1)
	{
		int len = 0;
		int parsed = 0;

		len = handler->input(buf + buf_len, handler->buf_size - buf_len, handler->context);
		if (len <= 0)
		{
			break;
		}
		buf_len += len;

		while (1)
		{
			int len = handler->output(buf + parsed, buf_len - parsed, handler->context);
			if (len <= 0)
			{
				break;
			}
			parsed += len;
		}

		memmove(buf, buf + parsed, buf_len - parsed);
		buf_len -= parsed;
	}

	free(buf);

	return 0;
}
