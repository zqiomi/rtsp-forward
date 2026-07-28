/**
 * @file rtsp_forward.h
 * @brief RTSP转发 对外接口头文件
 *
 * 轻量级RTSP转发库，支持一个输入多路转发，即外部输入RTP包，同时透传到多个RTSP客户端。
 *
 * 双线程模型支持：
 * - 主线程：调用 rtsp_forward_run() 运行事件循环，处理RTSP信令
 * - 码流线程：调用 rtsp_forward_send_rtp() 发送RTP包，线程安全
 *
 * 所有API返回int错误码：
 * - RTSP_OK (0): 成功
 * - 负数: 失败，具体错误码见RtspErrorCode枚举
 */

#ifndef RTSP_FORWARD_API_H_
#define RTSP_FORWARD_API_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * @brief RTSP转发 错误码定义
 */
typedef enum RtspErrorCode
{
    RTSP_OK = 0,                /**< 操作成功 */
    RTSP_ERROR = -1,            /**< 通用错误 */
    RTSP_INVALID_ARGUMENT = -2, /**< 参数无效（如NULL指针、非法值等） */
    RTSP_NETWORK_ERROR = -3,    /**< 网络错误（如端口冲突、bind失败等） */
    RTSP_CLOSED = -4,           /**< 连接已关闭 */
    RTSP_BUFFER_FULL = -5,      /**< 缓冲区满 */
    RTSP_PARSE_ERROR = -6,      /**< 协议解析错误 */
    RTSP_NOT_IMPLEMENTED = -7,  /**< 功能未实现 */
    RTSP_TIMEOUT = -8,          /**< 操作超时 */
    RTSP_LIMIT_EXCEEDED = -9,   /**< 超出限制（如最大会话数） */
    RTSP_OUT_OF_MEMORY = -10,   /**< 内存分配失败 */
    RTSP_ALREADY_STARTED = -11, /**< 服务器已启动 */
    RTSP_NOT_STARTED = -12,     /**< 服务器未启动 */
    RTSP_UNSUPPORTED_TRANSPORT = -13, /**< 不支持的传输协议 */
    RTSP_INTERNAL_ERROR = -14,        /**< 内部错误 */
} RtspErrorCode;

/**
 * @brief RTSP转发 配置结构体
 *
 * 用于创建服务器时传递配置参数。所有字段均为可选，未设置的字段将使用默认值。
 */
typedef struct RtspForwardConfig
{
    int port;                   /**< 监听端口，范围1-65535，默认554 */
    const char* ip;             /**< 监听地址，默认"0.0.0.0" */
    int max_sessions;           /**< 最大并发会话数，默认10 */
    size_t buffer_size;         /**< 每个连接的缓冲区大小（字节），默认65536 */
    const char* sdp_content;    /**< SDP内容，可为NULL（后续通过rtsp_forward_set_sdp设置） */
    int connection_timeout_sec; /**< 连接空闲超时（秒），适用于握手未完成的会话，0=不超时，默认30 */
    int session_timeout_sec;    /**< 会话空闲超时（秒），适用于已建立的会话，0=不超时，默认60 */
} RtspForwardConfig;

/**
 * @brief RTSP转发 服务器信息结构体
 *
 * 统一查询服务器的配置、运行状态与会话统计信息。
 */
typedef struct RtspForwardInfo
{
    int port;                    /**< 监听端口 */
    int max_sessions;            /**< 最大并发会话数 */
    int running;                 /**< 是否运行中（1=运行，0=未运行） */
    int active_sessions;         /**< 当前活跃会话数 */
    int playing_sessions;        /**< 当前播放中会话数 */
    uint64_t total_connections;  /**< 累计连接数 */
    uint64_t timed_out_sessions; /**< 因超时关闭的会话数 */
    uint64_t dropped_sessions;   /**< 因写缓冲区满（慢客户端）关闭的会话数 */
    uint64_t uptime_sec;         /**< 服务器运行时长（秒） */
} RtspForwardInfo;

/**
 * @brief 创建 RTSP 服务器实例
 *
 * @param[out] server 输出参数，用于返回创建的服务器句柄
 * @param[in] config 配置结构体指针，可为NULL（使用默认配置）
 * @return RTSP_OK表示成功，其他值表示失败
 *
 * @code
 * // 使用默认配置创建
 * void* server = NULL;
 * int ret = rtsp_forward_create(&server, NULL);
 * if (ret != RTSP_OK) {
 *     // 处理错误
 * }
 *
 * // 使用自定义配置创建
 * void* server = NULL;
 * RtspForwardConfig config = {
 *     .port = 554,
 *     .ip = "0.0.0.0",
 *     .max_sessions = 5,
 *     .buffer_size = 65536,
 *     .sdp_content = "v=0\r\n..."
 * };
 * int ret = rtsp_forward_create(&server, &config);
 * @endcode
 */
int rtsp_forward_create(void** server, const RtspForwardConfig* config);

/**
 * @brief 销毁 RTSP 服务器实例
 *
 * @param[in] server 服务器句柄，由rtsp_forward_create创建
 * @return RTSP_OK表示成功，其他值表示失败
 *
 * @note 调用此函数后，server句柄不再有效，不能再使用
 */
int rtsp_forward_destroy(void* server);

/**
 * @brief 启动 RTSP 服务器
 *
 * @param[in] server 服务器句柄
 * @return RTSP_OK表示成功，其他值表示失败
 *
 * @note 启动后需要调用rtsp_forward_run进入事件循环
 */
int rtsp_forward_start(void* server);

/**
 * @brief 停止 RTSP 服务器
 *
 * @param[in] server 服务器句柄
 * @return RTSP_OK表示成功，其他值表示失败
 *
 * @note 此函数会停止事件循环，关闭所有会话
 */
int rtsp_forward_stop(void* server);

/**
 * @brief 运行服务器事件循环（阻塞调用）
 *
 * @param[in] server 服务器句柄
 * @return RTSP_OK表示成功，其他值表示失败
 *
 * @note 此函数会阻塞当前线程，直到rtsp_forward_stop被调用或发生错误
 */
int rtsp_forward_run(void* server);

/**
 * @brief 发送 RTP 数据到所有播放中的会话
 *
 * 实现"一个输入，多路转发"的核心功能：输入一个RTP包，同时转发给所有处于PLAYING状态的会话。
 *
 * @param[in] server 服务器句柄
 * @param[in] data RTP数据指针（已包含完整RTP头）
 * @param[in] len 数据长度，必须大于0
 * @param[in] stream_index 流索引，0=RTP, 1=RTCP
 * @return RTSP_OK表示成功，其他值表示失败
 *
 * @code
 * // 发送RTP包到所有客户端
 * uint8_t rtp_data[1500];
 * size_t rtp_len = get_rtp_from_source(rtp_data, sizeof(rtp_data));
 * if (rtp_len > 0) {
 *     int ret = rtsp_forward_send_rtp(server, rtp_data, rtp_len, 0);
 * }
 * @endcode
 */
int rtsp_forward_send_rtp(void* server, const uint8_t* data, size_t len, int stream_index);

/**
 * @brief 设置 SDP 内容
 *
 * @param[in] server 服务器句柄
 * @param[in] sdp SDP内容字符串，以NULL结尾
 * @return RTSP_OK表示成功，其他值表示失败
 *
 * @note SDP内容将在客户端发送DESCRIBE请求时返回
 *
 * @code
 * const char* sdp = "v=0\r\n"
 *                   "o=- 0 0 IN IP4 0.0.0.0\r\n"
 *                   "s=RTSP Stream\r\n"
 *                   "c=IN IP4 0.0.0.0\r\n"
 *                   "t=0 0\r\n"
 *                   "m=video 0 RTP/AVP 96\r\n"
 *                   "a=rtpmap:96 H264/90000\r\n";
 * int ret = rtsp_forward_set_sdp(server, sdp);
 * @endcode
 */
int rtsp_forward_set_sdp(void* server, const char* sdp);

/**
 * @brief 获取服务器信息（配置、状态、统计）
 *
 * 统一查询服务器的监听端口、运行状态、会话统计等信息。
 *
 * @param[in] server 服务器句柄
 * @param[out] info 输出参数，服务器信息结构体
 * @return RTSP_OK表示成功，其他值表示失败
 *
 * @note 此函数线程安全，可在任意线程调用
 */
int rtsp_forward_get_info(void* server, RtspForwardInfo* info);

/**
 * @brief 获取版本号字符串
 *
 * @return 版本号字符串，如 "1.2.0"，静态存储，无需释放
 */
const char* rtsp_forward_version_string(void);

#ifdef __cplusplus
}
#endif

#endif  // RTSP_FORWARD_API_H_
