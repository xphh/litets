#include "litets.h"
#include <memory.h>

/************************************************************************/
/* default header data                                                  */
/************************************************************************/
static ps_pack_header g_pack_header;
static ps_system_header g_sys_header;
static ps_system_header_stream_table g_sys_streams[2];
static ps_map g_ps_map;

static void init_default_headers()
{
	static int s_initd = 0;
	if (s_initd)
	{
		return;
	}
	s_initd = 1;

	memset(&g_pack_header, 0, sizeof(g_pack_header));
	g_pack_header.start_code[0] = 0;
	g_pack_header.start_code[1] = 0;
	g_pack_header.start_code[2] = 1;
	g_pack_header.start_code[3] = 0xBA;
	g_pack_header.program_mux_rate[0] = 0;
	g_pack_header.program_mux_rate[1] = 0;
	g_pack_header.program_mux_rate[2] = 0x03;
	g_pack_header.pack_stuffing_length = 0xF8;

	memset(&g_sys_header, 0, sizeof(g_sys_header));
	g_sys_header.start_code[0] = 0;
	g_sys_header.start_code[1] = 0;
	g_sys_header.start_code[2] = 1;
	g_sys_header.start_code[3] = 0xBB;
	g_sys_header.header_length[0] = 0;
	g_sys_header.header_length[1] = 12;
	g_sys_header.rate_bound[0] = 0x80;
	g_sys_header.rate_bound[1] = 0x00;
	g_sys_header.rate_bound[2] = 0x01;
	g_sys_header.audio_bound = 1;
	g_sys_header.system_audio_lock_flag = 1;
	g_sys_header.system_video_lock_flag = 1;
	g_sys_header.marker_bit = 1;
	g_sys_header.video_bound = 1;
	g_sys_header.reserved_byte = 0xFF;

	memset(&g_sys_streams, 0, sizeof(g_sys_streams));
	// set all audio streams' buffer to 8KB
	g_sys_streams[0].stream_id = 0xB8;
	g_sys_streams[0].reserved = 3;
	g_sys_streams[0].P_STD_buffer_bound_scale = 0;
	g_sys_streams[0].P_STD_buffer_size_bound_high5 = 0x00;
	g_sys_streams[0].P_STD_buffer_size_bound_low8 = 0x40;
	// set all video streams' buffer to 512KB
	g_sys_streams[1].stream_id = 0xB9;
	g_sys_streams[1].reserved = 3;
	g_sys_streams[1].P_STD_buffer_bound_scale = 1;
	g_sys_streams[1].P_STD_buffer_size_bound_high5 = 0x02;
	g_sys_streams[1].P_STD_buffer_size_bound_low8 = 0x00;

	memset(&g_ps_map, 0, sizeof(g_ps_map));
	g_ps_map.start_code[0] = 0;
	g_ps_map.start_code[1] = 0;
	g_ps_map.start_code[2] = 1;
	g_ps_map.start_code[3] = 0xBC;
	g_ps_map.reserved1 = 3;
	g_ps_map.current_next_indicator = 1;
	g_ps_map.marker_bit = 1;
	g_ps_map.reserved2 = 127;
}

/************************************************************************/
/* 设置各种头部                                                         */
/************************************************************************/
static int set_pack_header(uint64_t pts, uint8_t *dest, int maxlen, int mux_rate)
{
	int hdrlen = sizeof(g_pack_header);
	ps_pack_header *hdr = (ps_pack_header *)dest;
	uint8_t *scr_buf = (uint8_t *)hdr->scr;

	if (maxlen < hdrlen)
	{
		return -1;
	}

	memcpy(hdr, &g_pack_header, hdrlen);

	scr_buf[0] = 0x40 | (((uint8_t)(pts >> 30) & 0x07) << 3) | 0x04 | ((uint8_t)(pts >> 28) & 0x03);
	scr_buf[1] = (uint8_t)((pts >> 20) & 0xff);
	scr_buf[2] = (((uint8_t)(pts >> 15) & 0x1f) << 3) | 0x04 | ((uint8_t)(pts >> 13) & 0x03);
	scr_buf[3] = (uint8_t)((pts >> 5) & 0xff);
	scr_buf[4] = (((uint8_t)pts & 0x1f) << 3) | 0x04;
	scr_buf[5] = 1;

	hdr->program_mux_rate[0] = ( mux_rate >> 14 ) & 0xff;
	hdr->program_mux_rate[1] = ( mux_rate >> 6 ) & 0xff;
	hdr->program_mux_rate[2] = ((( mux_rate<<2 ) & 0xfc ) | 0x03);

	return hdrlen;
}

static int set_sys_header(TsProgramSpec *spec, uint8_t *dest, int maxlen, int rate_bound)
{
	int hdrlen = sizeof(g_sys_header) + sizeof(g_sys_streams);
	ps_system_header *hdr = (ps_system_header *)dest;
	ps_system_header_stream_table *tbl = (ps_system_header_stream_table *)(hdr + 1);

	if (maxlen < hdrlen)
	{
		return -1;
	}
	
	memcpy(hdr, &g_sys_header, sizeof(g_sys_header));
	memcpy(tbl, &g_sys_streams, sizeof(g_sys_streams));

	// 数一下有几条音频流几条视频流
	do 
	{
		int i;
		int anum = 0;
		int vnum = 0;

		for (i = 0; i < spec->stream_num; i++)
		{
			if (lts_is_video(spec->stream[i].type))
				vnum++;
			else if (lts_is_audio(spec->stream[i].type))
				anum++;
		}
		
		hdr->audio_bound = anum;
		hdr->video_bound = vnum;

	} while (0);
	
	hdr->rate_bound[0] = (( rate_bound >> 15 ) & 0xff ) | 0x80;
	hdr->rate_bound[1] = ( rate_bound >> 7 ) & 0xff;
	hdr->rate_bound[2] = ((( rate_bound << 1) & 0xfe) | 0x01 );	
		
	return hdrlen;
}

static int set_psm(TsProgramSpec *spec, uint8_t *dest, int maxlen)
{
	ps_map *hdr = (ps_map *)dest;
	int es_num = spec->stream_num;
	int es_map_length = sizeof(ps_map_es) * es_num;
	int header_length = 6 + es_map_length + 4/*CRC*/;
	int hdrlen = sizeof(g_ps_map) + es_map_length + 4/*CRC*/;
	ps_map_es *es = (ps_map_es *)(hdr + 1);
	uint8_t *crc = (uint8_t *)(es + es_num);
	int i;

	if (maxlen < hdrlen)
	{
		return -1;
	}
	
	memcpy(hdr, &g_ps_map, sizeof(g_ps_map));
	hdr->es_map_length[0] = (char)(es_map_length >> 8);
	hdr->es_map_length[1] = (char)(es_map_length);
	hdr->header_length[0] = (char)(header_length >> 8);
	hdr->header_length[1] = (char)(header_length);

	// write es map
	memset(es, 0, es_map_length);
	for (i = 0; i < es_num; i++)
	{
		es[i].stream_type = spec->stream[i].type;
		es[i].es_id = lts_pes_stream_id(spec->stream[i].type, 0, i);
		es->es_info_length[0] = 0;
		es->es_info_length[1] = 0;
	}

	// VLC doesn't care about the CRC, it could be any 32-bits value.
	// If the CRC is needed in future, do reference to ISO-13818-1 Append I-B
	crc[0] = 0;
	crc[1] = 0;
	crc[2] = 0;
	crc[3] = 0;
	
	return hdrlen;
}

/************************************************************************/
/* 对外接口                                                             */
/************************************************************************/
int lts_ps_stream(TEsFrame *frame, uint8_t *dest, int maxlen, TsProgramInfo *pi)
{
	int i;
	int ret = 0;
	int ps_len = 0;
	TsProgramSpec *spec = &pi->prog[0];
	uint8_t *fdata;
	int flen;
	uint64_t pts;
	int PES_LEN, pes_num;
	uint8_t stream_id;
	SEGCALLBACK segcb;
	void *ctx;
	int mux_rate = spec->mux_rate;

	init_default_headers();

	if (!frame || !dest || !maxlen || !pi)
	{
		return -1;
	}
	
	if (!frame->frame || !frame->length)
	{
		return -1;
	}

	// callback
	segcb = frame->segcb;
	ctx = frame->ctx;

	// PS节目数只能为1
	if (pi->program_num != 1)
	{
		return -1;
	}
	
	// 设置ps头
	ret = set_pack_header(frame->pts, dest + ps_len, maxlen - ps_len, mux_rate);
	if (ret < 0)
	{
		return -1;
	}
		
	if (segcb)
		segcb(dest + ps_len, ret, ctx);
	ps_len += ret;

	// 只有外部设置了is_key，才会填PS各种头部
	if (frame->is_key)
	{
		// 设置系统头
		ret = set_sys_header(spec, dest + ps_len, maxlen - ps_len, mux_rate);
		if (ret < 0)
		{
			return -1;
		}
			
		if (segcb)
			segcb(dest + ps_len, ret, ctx);
		ps_len += ret;

		// 设置PSM
		ret = set_psm(spec, dest + ps_len, maxlen - ps_len);
		if (ret < 0)
		{
			return -1;
		}
			
		if (segcb)
			segcb(dest + ps_len, ret, ctx);
		ps_len += ret;
	}

	// 接下去加入PES包
	fdata = frame->frame;
	flen = frame->length;
	pts = frame->pts;
	PES_LEN = frame->ps_pes_length;
	if (PES_LEN < MIN_PES_LENGTH)
	{
		PES_LEN = MIN_PES_LENGTH;
	}
	else if (PES_LEN > MAX_PES_LENGTH)
	{
		PES_LEN = MAX_PES_LENGTH;
	}

	// 获得唯一的stream_id
	stream_id = lts_pes_stream_id(pi->prog[0].stream[frame->stream_number].type, 0, frame->stream_number);

	// 分别拷贝pes包头和es数据
	pes_num = (flen + PES_LEN - 1) / PES_LEN;
	for (i = 0; i < pes_num; i++)
	{
		int es_len = (i == pes_num - 1) ? (flen - PES_LEN * i) : PES_LEN;

		ret = lts_pes_make_header(stream_id, pts, es_len, dest + ps_len, maxlen - ps_len);
		if (ret < 0)
		{
			return -1;
		}
		
		memcpy(dest + ps_len + ret, fdata + PES_LEN * i, es_len);

		if (segcb)
			segcb(dest + ps_len, es_len + ret, ctx);

		ps_len += (es_len + ret);
	}

	return ps_len;
}
