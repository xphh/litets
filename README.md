# litets

非常轻型的TS和PS封装与解封装代码，严格遵循ISO/IEC 13818-1标准，扩展性好。

提供4个接口：
* lts_ts_stream：裸帧封装成TS流
* lts_ps_stream：裸帧封装成PS流
* lts_ts_demux：TS流解复用，分析每个TS包，得到PTS、ES数据等信息。
* lts_ps_demux：PS流解复用，分析每个PS包，得到PTS、ES数据等信息。

具体用法详见demo，里面展示了单条H264视频流与TS、PS的相互转化。多条流的情况，需要使用者自己关注时间戳的计算与音视频交织。

支持Windows（使用vs2005编译），支持Linux各种平台。

> 目前封装与解封装过程代码中无打印。如需定位码流问题，使用者可自行添加打印。也可以在vs中打断点调试，这样更方便。

