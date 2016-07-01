#ifndef __LITETS_STREAMDEF_LE_H__
#define __LITETS_STREAMDEF_LE_H__

#ifdef WIN32
#pragma pack(push) //保存对齐状态
#pragma pack(1)//设定为1字节对齐
#define __attribute__(x)
#endif

/************************************************************************/
/* little-endian                                                        */
/************************************************************************/
// PES Flags
typedef struct
{
	char orignal_or_copy: 1,
		 copyright: 1,
		 data_alignment_indicator: 1,
	 	 PES_priority: 1,
		 PES_scrambling_control: 2,
		 reserved: 2;
	char PES_extension_flag: 1,
		 PES_CRC_flag: 1,
		 additional_copy_info_flag: 1,
		 DSM_trick_mode_flag: 1,
		 ES_rate_flag: 1,
		 ESCR_flag: 1,
		 PTS_DTS_flags: 2;
} __attribute__((packed))
pes_head_flags;

// TS pure header
typedef struct
{
	char sync_byte;
	char PID_high5: 5,
		 transport_priority: 1,
		 payload_unit_start_indicator: 1,
		 transport_error_indicator: 1;
	char PID_low8;
	char continuity_counter: 4,
		 adaptation_field_control: 2,
		 transport_scrambling_control: 2;
} __attribute__((packed))
ts_pure_header;

// TS adaptation flags
typedef struct
{
	char adaptation_field_extension_flag: 1,
		 transport_private_data_flag: 1,
		 splicing_point_flag: 1,
		 OPCR_flag: 1,
		 PCR_flag: 1,
		 elementary_stream_priority_indicator: 1,
		 random_access_indicator: 1,
		 discontinuity_indicator: 1;
} __attribute__((packed))
ts_adaptation_flags;

// TS header
typedef struct
{
	ts_pure_header head;
	unsigned char adaptation_field_length;
	ts_adaptation_flags flags;
} __attribute__((packed))
ts_header;

// PAT
typedef struct
{
	char table_id;
	char section_length_high4: 4,
		 reserved1: 2,
		 zero: 1,
		 section_syntax_indicator: 1;
	char section_length_low8;
	char transport_stream_id_high8;
	char transport_stream_id_low8;
	char current_next_indicator: 1,
		 version_number: 5,
		 reserved2: 2;
	char section_number;
	char last_section_number;
} __attribute__((packed))
pat_section;

typedef struct
{
	char program_number_high8;
	char program_number_low8;
	char program_map_PID_high5: 5,
		 reserved: 3;
	char program_map_PID_low8;
} __attribute__((packed))
pat_map_array;

// PMT
typedef struct
{
	char table_id;
	char section_length_high4: 4,
		 reserved1: 2,
		 zero: 1,
		 section_syntax_indicator: 1;
	char section_length_low8;
	char program_number_high8;
	char program_number_low8;
	char current_next_indicator: 1,
		 version_number: 5,
		 reserved2: 2;
	char section_number;
	char last_section_number;
	char PCR_PID_high5: 5,
		 reserved3: 3;
	char PCR_PID_low8;
	char program_info_length_high4: 4,
		 reserved4: 4;
	char program_info_length_low8;
} __attribute__((packed))
pmt_section;

typedef struct
{
	char stream_type;
	char elementary_PID_high5: 5,
		 reserved1: 3;
	char elementary_PID_low8;
	char ES_info_length_high4: 4,
		 reserved2: 4;
	char ES_info_length_low8;
} __attribute__((packed))
pmt_stream_array;

/************************************************************************/
/* PS define:                                                           */
/************************************************************************/
typedef struct
{
	char start_code[4];				// == 0x000001BA
	char scr[6];
	char program_mux_rate[3];		// == 0x000003
	char pack_stuffing_length;		// == 0xF8
} __attribute__((packed))
ps_pack_header;

typedef struct  
{
	char start_code[4];				// == 0x000001BB
	char header_length[2];			// == 6 + 3 * stream_count(2)
	char rate_bound[3];				// == 0x800001
	char CSPS_flag: 1,				// == 0
		 fixed_flag: 1,				// == 0
		 audio_bound: 6;			// audio stream number
	char video_bound: 5,			// video stream number
		 marker_bit: 1,				// == 1
		 system_video_lock_flag: 1,	// == 1
		 system_audio_lock_flag: 1;	// == 1
	char reserved_byte;				// == 0xFF
} __attribute__((packed))
ps_system_header;

typedef struct
{
	// 0xB8 for all audio streams
	// 0xB9 for all video streams
	char stream_id;					
	char P_STD_buffer_size_bound_high5: 5,
		 // 0 for audio, scale x128B
		 // 1 for video, scale x1024B
		 P_STD_buffer_bound_scale: 1,
		 reserved: 2;				// == 3
	char P_STD_buffer_size_bound_low8;
} __attribute__((packed))
ps_system_header_stream_table;

// PSM
typedef struct
{
	char start_code[4];				// == 0x000001BC
	char header_length[2];			// == 6 + es_map_length
	char ps_map_version: 5,			// == 0
		 reserved1: 2,				// == 3
		 current_next_indicator: 1;	// == 1
	char marker_bit: 1,				// == 1
		 reserved2: 7;				// == 127
	char ps_info_length[2];			// == 0
	char es_map_length[2];			// == 4 * es_num
} __attribute__((packed))
ps_map;

typedef struct
{
	char stream_type;
	char es_id;
	char es_info_length[2];			// == 0
} __attribute__((packed))
ps_map_es;

#ifdef _MSC_VER
#pragma pack(pop)//恢复对齐状态
#endif

#endif //__LITETS_STREAMDEF_LE_H__
