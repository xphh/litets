#ifndef __LITETS_TSSTREAM_H__
#define __LITETS_TSSTREAM_H__

#ifdef WIN32
#include <basetsd.h>
typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
#define __LITTLE_ENDIAN		1
#define __BIG_ENDIAN		2
#define __BYTE_ORDER		1
#else
#include <inttypes.h>
#include <endian.h>
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#include "streamdef_le.h"
#elif __BYTE_ORDER == __BIG_ENDIAN
#error "Temporarily not support big-endian systems."
#else
#error "Please fix <endian.h>"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************/
/* 实体流相关定义                                                       */
/************************************************************************/
#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_AUDIO_AAC_LATM  0x11
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_VIDEO_H264      0x1b
#define STREAM_TYPE_VIDEO_HEVC      0x24
#define STREAM_TYPE_VIDEO_CAVS      0x42
#define STREAM_TYPE_VIDEO_VC1       0xea
#define STREAM_TYPE_VIDEO_DIRAC     0xd1
#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x82
#define STREAM_TYPE_AUDIO_TRUEHD    0x83

typedef void (*SEGCALLBACK)(uint8_t *buf, int len, void *ctx);

#define MIN_PES_LENGTH	(1000)
#define MAX_PES_LENGTH	(65000)

// 实体流帧信息
typedef struct
{
	int program_number; // 节目编号，就是TsProgramInfo中prog数组下标，对于PS该值只能为0
	int stream_number;	// 流编号，就是TsProgramSpec中stream数组下标
	uint8_t *frame;		// 帧数据
	int length;			// 帧长度
	int is_key;			// 当前帧TS流化时是否带PAT和PMT
	uint64_t pts;		// 时间戳, 90kHz
	int ps_pes_length;	// 需要切分成PES的长度，该参数只对PS有效，最大不能超过MAX_PES_LENGTH
	SEGCALLBACK segcb;	// 对于PS，当生成一段数据（头部或PES）回调，不用可设为NULL
	void *ctx;			// 回调上下文
} TEsFrame;

// 判断是否视频
int lts_is_video(int type);
// 判断是否音频
int lts_is_audio(int type);

// 确定PES中的stream_id
uint8_t lts_pes_stream_id(int type, int program_number, int stream_number);

// 生成PES头部，返回头部总长度
int lts_pes_make_header(uint8_t stream_id, uint64_t pts, int es_len, uint8_t *dest, int maxlen);

// 解析PES头部长度，返回头部总长度
int lts_pes_parse_header(uint8_t *pes, int len, uint8_t *stream_id, uint64_t *pts, int *es_len);

/************************************************************************/
/* 节目信息定义                                                         */
/************************************************************************/
// 每条流的详情
typedef struct
{
	uint8_t type;			// [I]媒体类型
	uint8_t stream_id;		// [O]实体流ID（与PES头部id相同）
	int es_pid;				// [O]实体流的PID
	int continuity_counter;	// [O] TS包头部的连续计数器, 外部需要维护这个计数值, 必须每次传入上次传出的计数值
} TsStreamSpec;

// 每个节目的详情
#define MAX_STREAM_NUM		(4)

typedef struct
{
	int stream_num;			// [I]这个节目包含的流个数
	int key_stream_id;		// {I]基准流编号
	int pmt_pid;			// [O]这个节目对应的PMT表的PID（TS解码用）
	int mux_rate;			// [O]这个节目的码率，单位为50字节每秒(PS编码用)
	TsStreamSpec stream[MAX_STREAM_NUM];
} TsProgramSpec;

// 节目信息（目前最多支持1个节目2条流）
#define MAX_PROGRAM_NUM		(1)
typedef struct
{
	int program_num;		// [I]这个TS流包含的节目个数，对于PS该值只能为1
	int pat_pmt_counter;	// [O]PAT、PMT计数器
	TsProgramSpec prog[MAX_PROGRAM_NUM];
} TsProgramInfo;

/************************************************************************/
/* 接口函数，将帧流化。必须指定TsProgramInfo							*/
/************************************************************************/

// 返回流化后TS的长度，出错（如dest空间不足）返回-1。 
int lts_ts_stream(TEsFrame *frame, uint8_t *dest, int maxlen, TsProgramInfo *pi);

// 返回PS的长度，出错（如dest空间不足）返回-1。 
int lts_ps_stream(TEsFrame *frame, uint8_t *dest, int maxlen, TsProgramInfo *pi);

/************************************************************************/
/* 接口函数，组帧。                                                     */
/************************************************************************/
typedef struct  
{
	TsProgramInfo info;		// 节目信息
	int is_pes;				// 属于数据，不是PSI
	int pid;				// 当前包的PID
	int program_no;			// 当前包所属的节目号
	int stream_no;			// 当前包所属的流号
	uint64_t pts;			// 当前包的时间戳
	uint64_t pes_pts;		// 当前PES的时间戳
	uint8_t *pack_ptr;		// 解出一包的首地址
	int pack_len;			// 解出一包的长度
	uint8_t *es_ptr;		// ES数据首地址
	int es_len;				// ES数据长度
	int pes_head_len;		// PES头部长度
	int sync_only;			// 只同步包，不解析包
	int ps_started;			// 已找到PS头部
} TDemux;

// TS码流解复用，成功返回已处理长度，失败返回-1
// ts_buf是传入的TS流的缓冲
int lts_ts_demux(TDemux *handle, uint8_t *ts_buf, int len);

// PS码流解复用，成功返回已处理长度，失败返回-1
// ps_buf是传入的PS流的缓冲
int lts_ps_demux(TDemux *handle, uint8_t *ps_buf, int len);

/************************************************************************/
/* 缓存处理辅助接口                                                     */
/************************************************************************/
typedef struct
{
	int buf_size;
	int (*input)(uint8_t *buf, int size, void *context);
	int (*output)(uint8_t *buf, int size, void *context);
	void *context;
} TBufferHandler;

int lts_buffer_handle(TBufferHandler *handler);

#ifdef __cplusplus
}
#endif

#endif //__LITETS_TSSTREAM_H__
