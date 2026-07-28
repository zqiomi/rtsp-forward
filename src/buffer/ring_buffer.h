#ifndef RTSP_FORWARD_RING_BUFFER_H_
#define RTSP_FORWARD_RING_BUFFER_H_

#include <sys/uio.h>

#include <cstddef>
#include <memory>

#include "util/status.h"

namespace rtsp_forward
{

// 环形缓冲区类
class RingBuffer
{
public:
    // 构造函数
    explicit RingBuffer(size_t capacity);

    // 析构函数
    ~RingBuffer();

    // 禁止拷贝和移动
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    // 写入数据
    Status Write(const void* data, size_t size);

    // 查看数据（不移动读指针）
    Status Peek(void* data, size_t size) const;

    // 获取可读数据大小
    size_t ReadableSize() const;

    // 获取可写空间大小
    size_t WritableSize() const;

    // 获取从 WritePtr 起的连续可写空间大小（不越过 buffer 末尾，避免环绕导致越界写）
    size_t ContiguousWritableSize() const;

    // 获取从 ReadPtr 起的连续可读空间大小（不越过 buffer 末尾，避免环绕导致越界读）
    size_t ContiguousReadableSize() const;

    // 获取读指针位置的数据（用于直接读取）
    const char* ReadPtr() const;

    // 获取写指针位置（用于直接写入）
    char* WritePtr();

    // 确认读取了多少数据
    void Consume(size_t size);

    // 确认写入了多少数据
    void Produce(size_t size);

    // 在可读数据中查找指定字符，返回相对于读指针的偏移量
    // max_search_len: 最大搜索长度，0 表示搜索全部可读数据
    // 未找到返回 npos
    size_t FindChar(char c, size_t max_search_len = 0) const;

    size_t FindSubstring(const char* substr, size_t substr_len, size_t max_search_len = 0) const;

    // 填充 iovec 数组，返回有效条目数（1 或 2）
    // iov[0] = 从 read_pos_ 起的连续可读段
    // iov[1] = 环绕到 buffer 开头的剩余段（仅当数据环绕时存在）
    int GetReadableIoVec(struct iovec* iov) const;

    static constexpr size_t npos = static_cast<size_t>(-1);

private:
    std::unique_ptr<char[]> buffer_;  // 缓冲区内存
    size_t capacity_;                 // 总容量
    size_t read_pos_;                 // 读指针位置
    size_t write_pos_;                // 写指针位置
    size_t readable_size_;            // 当前可读数据量
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_RING_BUFFER_H_
