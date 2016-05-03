#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include "litets.h"

#define BUF_SIZE (1<<20)

static int g_is_ps = 0;
static FILE *g_in_fp;
static FILE *g_out_fp;
static TDemux g_demux;
static TsProgramInfo g_prog_info;
static int g_frame_count = 0;
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

static int streaming_input(uint8_t *buf, int size, void *context)
{
	return (int)fread(buf, 1, size, g_in_fp);
}

static int streaming_output(uint8_t *buf, int size, void *context)
{
	int start_pos = find_starter(buf, size, 0);
	if (start_pos < 0)
	{
		return 0;
	}

	int end_pos = find_starter(buf, size, start_pos + 4);
	if (end_pos < 0)
	{
		return 0;
	}

	uint8_t *data_ptr = buf + start_pos;
	int data_len = end_pos - start_pos;
	int is_key = 0;

	printf("[%d] type[%02x] frame len = %d\n", g_frame_count, data_ptr[4], data_len);

	// 简单认为小于100字节的帧是SPS、PPS等非数据帧。正式使用应根据H264帧类型严格判断。
	if (data_len < 100)
	{
		is_key = 1;
	}
	else
	{
		g_frame_count++;
	}

	TEsFrame es = {0};
	es.program_number = 0;
	es.stream_number = 0;
	es.frame = data_ptr;
	es.length = data_len;
	es.is_key = is_key;					// 这里简单处理，认为信息帧（非数据帧）为关键帧。
	es.pts = 3600L * g_frame_count;		// 示例中按帧率为25fps累计时间戳。正式使用应根据帧实际的时间戳填写。
	es.ps_pes_length = 8000;

	int outlen = 0;
	if (g_is_ps)
	{
		outlen = lts_ps_stream(&es, g_outbuf, BUF_SIZE, &g_prog_info);
	}
	else
	{
		outlen = lts_ts_stream(&es, g_outbuf, BUF_SIZE, &g_prog_info);
	}

	if (outlen > 0)
	{
		fwrite(g_outbuf, 1, outlen, g_out_fp);
	}

	return end_pos;
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

	g_in_fp = fopen(filename, "rb");
	if (!g_in_fp)
	{
		printf("read file failed!\n");
		return;
	}

	g_out_fp = fopen(outfile, "wb");
	if (!g_out_fp)
	{
		printf("open output file failed!\n");
		return;
	}

	memset(&g_prog_info, 0, sizeof(g_prog_info));
	g_prog_info.program_num = 1;
	g_prog_info.prog[0].stream_num = 1;
	g_prog_info.prog[0].stream[0].type = STREAM_TYPE_VIDEO_H264;
	
	TBufferHandler handler;
	handler.buf_size = BUF_SIZE;
	handler.input = streaming_input;
	handler.output = streaming_output;
	handler.context = NULL;
	lts_buffer_handle(&handler);

	fclose(g_in_fp);

	fclose(g_out_fp);
}

static int decoding_input(uint8_t *buf, int size, void *context)
{
	return (int)fread(buf, 1, size, g_in_fp);
}

static int decoding_output(uint8_t *buf, int size, void *context)
{
	int len = 0;

	if (g_is_ps)
	{
		len = lts_ps_demux(&g_demux, buf, size);
	}
	else
	{
		len = lts_ts_demux(&g_demux, buf, size);
	}

	if (len > 0)
	{
		printf("[%d/%d] is_pes[%d] es_len = %d, pts = %lld\n", 
			g_demux.program_no, g_demux.stream_no, g_demux.is_pes, g_demux.es_len, (long long)g_demux.pts);

		if (g_demux.is_pes)
		{
			fwrite(g_demux.es_ptr, 1, g_demux.es_len, g_out_fp);
		}
	}

	return len;
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

	g_in_fp = fopen(filename, "rb");
	if (!g_in_fp)
	{
		printf("read file failed!\n");
		return;
	}

	g_out_fp = fopen(outfile, "wb");
	if (!g_out_fp)
	{
		printf("open output file failed!\n");
		return;
	}

	memset(&g_demux, 0, sizeof(g_demux));

	TBufferHandler handler;
	handler.buf_size = BUF_SIZE;
	handler.input = decoding_input;
	handler.output = decoding_output;
	handler.context = NULL;
	lts_buffer_handle(&handler);

	fclose(g_in_fp);

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
