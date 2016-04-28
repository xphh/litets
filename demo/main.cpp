#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include "litets.h"

#define BUF_SIZE (1<<20)

static FILE *g_out_fp;
static int g_is_ps = 0;
static uint8_t *g_inbuf = new uint8_t[BUF_SIZE];
static uint8_t *g_outbuf = new uint8_t[BUF_SIZE];


static int find_starter(uint8_t *buf, int size, int from)
{
	for (int i = from; i < size - 4; i++)
	{
		if (buf[i] == 0 && buf[i+1] == 0 && buf[i+2] == 0 && buf[i+3] == 1)
		{
			return i;
		}
	}
	return -1;
}

typedef void (*FRAMECB)(int count, int is_frame, uint8_t *data, int len, void *ctx);

static int read_raw_frame(const char *filename, FRAMECB cb, void *ctx)
{
	FILE *fp = fopen(filename, "rb");
	if (!fp)
	{
		printf("read file failed.\n");
		return -1;
	}

	int buf_len = 0;
	int start_pos = -1;
	int count = 0;

	while (1)
	{
		int len = (int)fread(g_inbuf + buf_len, 1, BUF_SIZE - buf_len, fp);
		if (len <= 0)
		{
			break;
		}
		buf_len += len;

		if (start_pos < 0)
		{
			start_pos = find_starter(g_inbuf, buf_len, 0);
		}

		int end_pos = find_starter(g_inbuf, buf_len, start_pos + 4);
		if (end_pos < 0)
		{
			buf_len = 0;
			start_pos = -1;
		}
		else
		{
			uint8_t *data_ptr = g_inbuf + start_pos;
			int data_len = end_pos - start_pos;
			// 简单认为小于100字节的帧是SPS、PPS等非数据帧。正式使用应根据H264帧类型严格判断。
			if (data_len < 100)
			{
				cb(count, 0, data_ptr, data_len, ctx);
			}
			else
			{
				cb(count, 1, data_ptr, data_len, ctx);
				count++;
			}

			memmove(g_inbuf, g_inbuf + end_pos, buf_len - end_pos);
			buf_len -= end_pos;
			start_pos = 0;
		}
	}

	fclose(fp);

	return 0;
}

static void get_one_frame(int count, int is_frame, uint8_t *data, int len, void *ctx)
{
	TsProgramInfo *p = (TsProgramInfo *)ctx;

	printf("[%d] frame len = %d\n", count, len);

	TEsFrame es = {0};
	es.program_number = 0;
	es.stream_number = 0;
	es.frame = data;
	es.length = len;
	es.is_key = is_frame ? 0 : 1;	// 这里简单处理，认为信息帧（非数据帧）为关键帧。
	es.pts = 3600L * count;			// 示例中按帧率为25fps累计时间戳。正式使用应根据帧实际的时间戳填写。
	es.ps_pes_length = 8000;

	int outlen = 0;
	if (g_is_ps)
	{
		outlen = lts_ps_stream(&es, g_outbuf, BUF_SIZE, p);
	}
	else
	{
		outlen = lts_ts_stream(&es, g_outbuf, BUF_SIZE, p);
	}

	if (outlen > 0)
	{
		fwrite(g_outbuf, 1, outlen, g_out_fp);
	}
}

void do_streaming(const char *filename)
{
	char outfile[256] = {0};
	if (g_is_ps)
	{
		sprintf(outfile, "%s.ps", filename);
	}
	else
	{
		sprintf(outfile, "%s.ts", filename);
	}

	g_out_fp = fopen(outfile, "wb");
	if (!g_out_fp)
	{
		printf("open output file failed!\n");
		return;
	}

	TsProgramInfo info = {0};
	info.program_num = 1;
	info.prog[0].stream_num = 1;
	info.prog[0].stream[0].type = EsFrame_H264;
	read_raw_frame(filename, get_one_frame, &info);

	fclose(g_out_fp);
}

void do_decoding(const char *filename)
{
	if (strcmp(filename + strlen(filename) - 3, ".ps") == 0)
	{
		g_is_ps = 1;
	}
	else if (strcmp(filename + strlen(filename) - 3, ".ts") == 0)
	{
		g_is_ps = 0;
	}
	else
	{
		printf("input file extension name must be 'ts' or 'ps'!\n");
		return;
	}

	char outfile[256] = {0};
	sprintf(outfile, "%s.es", filename);

	g_out_fp = fopen(outfile, "wb");
	if (!g_out_fp)
	{
		printf("open output file failed!\n");
		return;
	}

	FILE *fp = fopen(filename, "rb");
	if (!fp)
	{
		printf("read file failed!\n");
		return;
	}

	TDemux demux;
	memset(&demux, 0, sizeof(demux));

	int buflen = 0;

	while (1)
	{
		int len = (int)fread(g_inbuf + buflen, 1, BUF_SIZE - buflen, fp);
		if (len <= 0)
		{
			break;
		}
		buflen += len;

		int parsed = 0;
		while (1)
		{
			int len = 0;
			if (g_is_ps)
			{
				len = lts_ps_demux(&demux, g_inbuf + parsed, buflen - parsed);
			}
			else
			{
				len = lts_ts_demux(&demux, g_inbuf + parsed, buflen - parsed);
			}
			if (len <= 0)
			{
				break;
			}
			parsed += len;

			printf("[%d/%d] is_pes[%d] es_len = %d, pts = %lld\n", demux.program_no, demux.stream_no, demux.is_pes, demux.es_len, (long long)demux.pts);

			if (demux.is_pes)
			{
				fwrite(demux.es_ptr, 1, demux.es_len, g_out_fp);
			}
		}

		memmove(g_inbuf, g_inbuf + parsed, buflen - parsed);
		buflen -= parsed;
	}

	fclose(fp);

	fclose(g_out_fp);
}

void show_usage(const char *name)
{
	printf("usage:\n");
	printf("  for ts streaming: %s -ts input_file\n", name);
	printf("  for ps streaming: %s -ps input_file\n", name);
	printf("  for     demuxing: %s -d  input_file\n", name);
	getchar();
}

int main(int argc, const char *argv[])
{
	if (argc != 3)
	{
		show_usage(argv[0]);
		return 0;
	}

	const char *cmd = argv[1];
	const char *input_file = argv[2];

	if (strcmp(cmd, "-ts") == 0)
	{
		do_streaming(input_file);
	}
	else if (strcmp(cmd, "-ps") == 0)
	{
		g_is_ps = 1;
		do_streaming(input_file);
	}
	else if (strcmp(cmd, "-d") == 0)
	{
		do_decoding(input_file);
	}
	else
	{
		show_usage(argv[0]);
		return 0;
	}

	return 0;
}
