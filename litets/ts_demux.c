#include "litets.h"
#include <stdio.h>

static int get_ts_payload_offset(uint8_t *ts_pack)
{
	ts_header *ts = (ts_header *)ts_pack;
	int adaptation_field_control = ts->head.adaptation_field_control;
	int payload_offset;
	
	if (adaptation_field_control == 0)
		payload_offset = 188;
	else if (adaptation_field_control == 1)
		payload_offset = 4;
	else if (adaptation_field_control == 2)
		payload_offset = 188;
	else
	{
		payload_offset = 5 + ts->adaptation_field_length;
		if (payload_offset > 188)
			payload_offset = 188;
		else if (payload_offset < 5)
			payload_offset = 5;
	}
	
	return payload_offset;
}

static void get_ts_pcr(uint8_t *ts_pack, uint64_t *pcr)
{
	ts_header *ts = (ts_header *)ts_pack;
	
	if (ts->head.adaptation_field_control == 0 ||
		ts->head.adaptation_field_control == 1)
		return;
	if (ts->adaptation_field_length == 0)
		return;
	
	if (ts->flags.PCR_flag)
	{
		uint8_t *buf = &ts_pack[6];
		*pcr = ((uint64_t)buf[0] << 25) |
			((uint64_t)buf[1] << 17) |
			((uint64_t)buf[2] << 9) |
			((uint64_t)buf[3] << 1) |
			((uint64_t)buf[4] >> 7);
		*pcr /= 90;
	}
}

static void get_ts_es(TDemux *handle, uint8_t *ts_pack)
{
	int payload_offset;
	int payload_len;

	payload_offset = get_ts_payload_offset(ts_pack);
	payload_len = 188 - payload_offset;

	// 计算PES包头长度
	if (handle->pes_head_len <= 0)
	{
		handle->pes_head_len = lts_pes_parse_header(ts_pack + payload_offset, payload_len, NULL, &handle->pes_pts, NULL);
	}

	// 去除PES包头
	if (handle->pes_head_len > 0)
	{
		if (handle->pes_head_len <= payload_len)
		{
			handle->es_ptr = ts_pack + payload_offset + handle->pes_head_len;
			handle->es_len = payload_len - handle->pes_head_len;
			handle->pes_head_len = 0;
		}
		else
		{
			handle->pes_head_len -= payload_len;
			handle->es_ptr = NULL;
			handle->es_len = 0;
		}
	}
	else
	{
		handle->es_ptr = ts_pack + payload_offset;
		handle->es_len = payload_len;
	}
}


#define CHAR_TO_LENGTH(h, l)	(((int)(uint8_t)(h) << 8) | (uint8_t)(l))

static int handle_ts_pack(TDemux *handle, uint8_t *ts_pack)
{
	int i;
	ts_header *ts = (ts_header *)ts_pack;
	int PID = CHAR_TO_LENGTH(ts->head.PID_high5, ts->head.PID_low8);
	handle->pid = PID;

	// 如果是PAT表，则更新handle
	// 不支持过长的PAT或PMT表
	if (PID == 0)
	{
		int payload_offset = get_ts_payload_offset(ts_pack) + 1;
		uint8_t *payload = ts_pack + payload_offset;
		int payload_len = 188 - payload_offset;
		if (payload_len >= sizeof(pat_section))
		{
			pat_section *pat = (pat_section *)payload;
			int section_len = CHAR_TO_LENGTH(pat->section_length_high4, pat->section_length_low8);
			if (payload_len >= section_len + 3)
			{
				int pnum = (payload_len - 4 - sizeof(pat_section)) / sizeof(pat_map_array);
				pat_map_array *maps = (pat_map_array *)(payload + sizeof(pat_section));

				if (pnum > MAX_PROGRAM_NUM)
					pnum = MAX_PROGRAM_NUM;
				handle->info.program_num = pnum;

				for (i = 0; i < pnum; i++)
				{
					// 最多支持MAX_PROGRAM_NUM张PMT表
					if (i >= MAX_PROGRAM_NUM)
						break;

					handle->info.prog[i].pmt_pid = 
						CHAR_TO_LENGTH(maps[i].program_map_PID_high5, maps[i].program_map_PID_low8);
				}
			}
		}

		return 0;
	}

	// 查找是否是某个节目的PMT表
	for (i = 0; i < handle->info.program_num; i++)
	{
		if (PID == handle->info.prog[i].pmt_pid)
		{
			int payload_offset = get_ts_payload_offset(ts_pack) + 1;
			uint8_t *payload = ts_pack + payload_offset;
			int payload_len = 188 - payload_offset;
			if (payload_len > sizeof(pmt_section))
			{
				pmt_section *pmt = (pmt_section *)payload;
				int section_len = CHAR_TO_LENGTH(pmt->section_length_high4, pmt->section_length_low8);
				if (payload_len >= section_len + 3)
				{
					int program_info_length = CHAR_TO_LENGTH(pmt->program_info_length_high4, pmt->program_info_length_low8);
					pmt_stream_array *stm = (pmt_stream_array *)(payload + sizeof(pmt_section) + program_info_length);
					int sn;
					for (sn = 0; sn < MAX_STREAM_NUM; sn++)
					{
						if ((uint8_t *)stm + 4 >= payload + 3 + section_len) {
							break;
						}
						handle->info.prog[i].stream[sn].es_pid = 
							CHAR_TO_LENGTH(stm->elementary_PID_high5, stm->elementary_PID_low8);
						handle->info.prog[i].stream[sn].type = stm->stream_type;

						stm = (pmt_stream_array *)((uint8_t *)stm + sizeof(pmt_stream_array)
							+ CHAR_TO_LENGTH(stm->ES_info_length_high4, stm->ES_info_length_low8));
					}
					handle->info.prog[i].stream_num = sn;
				}
			}

			return 0;
		}
	}

	// 查找是否是某个节目的某条流
	for (i = 0; i < handle->info.program_num; i++)
	{
		int sn;
		for (sn = 0; sn < handle->info.prog[i].stream_num; sn++)
		{
			if (PID == handle->info.prog[i].stream[sn].es_pid)
			{
				handle->program_no = i;
				handle->stream_no = sn;
				get_ts_pcr(ts_pack, &handle->pts);
				handle->is_pes = 1;
				get_ts_es(handle, ts_pack);
				return 0;
			}
		}
	}

	// 未找到
	return 0;
}

int lts_ts_demux(TDemux *handle, uint8_t *ts_buf, int len)
{
	int ret = 0;
	int i;

	if (!handle || !ts_buf || len <= 0)
	{
		return -1;
	}

	handle->is_pes = 0;
	handle->pack_ptr = NULL;
	handle->pack_len = 0;

	// 只解出最开头一包
	for (i = 0; i < len; i++)
	{
		if (ts_buf[i] == 0x47)
		{
			if (len >= 188)
			{
				handle->pack_ptr = ts_buf + i;
				handle->pack_len = 188;
				if (!handle->sync_only)
				{
					handle_ts_pack(handle, ts_buf + i);
				}
				ret = i + 188;
				break;
			}
		}
	}
	
	return ret;
}
