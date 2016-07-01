#include "litets.h"
#include <memory.h>

// 确定PES中的stream_id
uint8_t lts_pes_stream_id(int type, int program_number, int stream_number)
{
	uint8_t id = 0;

	if (lts_is_video(type))
		id = 0xE0 + program_number * MAX_STREAM_NUM + stream_number;
	else if (lts_is_audio(type))
		id = 0xC0 + program_number * MAX_STREAM_NUM + stream_number;

	return id;
}

/************************************************************************/
/* make                                                                 */
/************************************************************************/

// 生成PES头中的PTS和DTS字段
static void make_pes_pts_dts(uint8_t *buff, uint64_t ts)
{
	// PTS
	buff[0] = (uint8_t)(((ts >> 30) & 0x07) << 1) | 0x30 | 0x01;
	buff[1] = (uint8_t)((ts >> 22) & 0xff);
	buff[2] = (uint8_t)(((ts >> 15) & 0xff) << 1) | 0x01;
	buff[3] = (uint8_t)((ts >> 7) & 0xff);
	buff[4] = (uint8_t)((ts & 0xff) << 1) | 0x01;
	// DTS
	buff[5] = (uint8_t)(((ts >> 30) & 0x07) << 1) | 0x10 | 0x01;
	buff[6] = (uint8_t)((ts >> 22) & 0xff);
	buff[7] = (uint8_t)(((ts >> 15) & 0xff) << 1) | 0x01;
	buff[8] = (uint8_t)((ts >> 7) & 0xff);
	buff[9] = (uint8_t)((ts & 0xff) << 1) | 0x01;
}

// 生成一个带PTS和DTS的PES头部
// 目前当pes_packet_length超过2B, 设置为0, 对于TS, 标准允许设置为0
int lts_pes_make_header(uint8_t stream_id, uint64_t pts, int es_len, uint8_t *dest, int maxlen)
{
	uint8_t *buf = dest;
	pes_head_flags flags = {0};
	int pes_packet_length = es_len + 13;

	if (pes_packet_length > 65535)
	{
		pes_packet_length = 0;
	}

	if (maxlen > 0 && (19 + es_len) > maxlen)
	{
		return -1;
	}

	// prefix
	*buf++ = 0;
	*buf++ = 0;
	*buf++ = 1;

	// stream_id
	*buf++ = stream_id;

	// pes length
	*buf++ = (uint8_t)(pes_packet_length >> 8);
	*buf++ = (uint8_t)pes_packet_length;

	// pes head flags
	flags.reserved = 2;
	flags.PTS_DTS_flags = 3;
	memcpy(buf, &flags, 2);
	buf += 2;

	// pes head length
	*buf++ = 10;

	// PTS & DTS
	make_pes_pts_dts(buf, pts);

	return 19;
}

/************************************************************************/
/* parse                                                                */
/************************************************************************/
#define BUF2U16(buf) (((buf)[0] << 8) | (buf)[1])

// 解析PES总长
static int get_pes_head_len(uint8_t *pes, int len)
{
	int pes_head_len = 0;

	if (len < 9)
	{
		return 0;
	}

	if (pes[0] == 0 && pes[1] == 0 && pes[2] == 1)
	{
		if ((pes[3] & 0xC0) || (pes[3] & 0xE0))
		{
			pes_head_len = 9 + pes[8];
		}
	}

	return pes_head_len;
}

// 解析PES头部长度，返回头部总长度
int lts_pes_parse_header(uint8_t *pes, int len, uint8_t *stream_id, uint64_t *pts, int *es_len)
{
	int pes_head_len = get_pes_head_len(pes, len);
	if (pes_head_len <= 0)
	{
		return 0;
	}

	if (stream_id)
	{
		*stream_id = pes[3];
	}

	// 解析PTS
	if (pts)
	{
		uint8_t flags_2 = pes[7];
		if (flags_2 & 0x80)
		{
			uint8_t *pts_buf = &pes[9];
			*pts  = ((uint64_t)pts_buf[0] & 0x0E) << 29;
			*pts |= ((uint64_t)pts_buf[1]       ) << 22;
			*pts |= ((uint64_t)pts_buf[2] & 0xFE) << 14;
			*pts |= ((uint64_t)pts_buf[3]       ) <<  7;
			*pts |= ((uint64_t)pts_buf[4] & 0xFE) >>  1;
			*pts /= 90;
		}
	}

	// 解析ES长度
	if (es_len)
	{
		int pes_len = BUF2U16(&pes[4]);
		int pes_head_len = pes[8];
		*es_len = pes_len - 3 - pes_head_len;
	}

	return pes_head_len;
}
