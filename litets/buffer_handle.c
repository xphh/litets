#include "litets.h"
#include <string.h>
#include <malloc.h>

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
