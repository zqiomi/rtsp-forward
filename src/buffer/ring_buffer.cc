#include "ring_buffer.h"

#include <cstdlib>
#include <cstring>

#include "util/log.h"

namespace rtsp_forward
{

RingBuffer::RingBuffer(size_t capacity)
    : capacity_(capacity > 0 ? capacity : 1), read_pos_(0), write_pos_(0), readable_size_(0)
{
    char* buf = new (std::nothrow) char[capacity_];
    if (!buf)
    {
        LOG_FATAL("RingBuffer: memory allocation failed, capacity=%zu", capacity_);
        std::abort();
    }
    buffer_ = std::unique_ptr<char[]>(buf);
    LOG_DEBUG("RingBuffer[%p] created, capacity=%zu", this, capacity_);
}

RingBuffer::~RingBuffer()
{
    LOG_DEBUG("RingBuffer[%p] destroyed", this);
}

Status RingBuffer::Write(const void* data, size_t size)
{
    if (!data || size == 0)
    {
        return Status::InvalidArgument("invalid data or size");
    }

    if (size > WritableSize())
    {
        return Status::BufferFull("buffer full");
    }

    const char* src = static_cast<const char*>(data);

    // 需要环绕写入时拆成两段；否则单次拷贝
    if (size > capacity_ - write_pos_)
    {
        size_t first_part = capacity_ - write_pos_;
        memcpy(buffer_.get() + write_pos_, src, first_part);
        memcpy(buffer_.get(), src + first_part, size - first_part);
    }
    else
    {
        memcpy(buffer_.get() + write_pos_, src, size);
    }

    // 与 Produce() 一致：用模运算保证 write_pos_ 始终落在 [0, capacity_)
    write_pos_ = (write_pos_ + size) % capacity_;
    readable_size_ += size;
    return Status::Ok();
}

Status RingBuffer::Peek(void* data, size_t size) const
{
    if (!data || size == 0)
    {
        return Status::InvalidArgument("invalid data or size");
    }

    if (size > readable_size_)
    {
        return Status::Error("not enough data");
    }

    char* dst = static_cast<char*>(data);

    // 环绕读取时拆成两段；Peek 不移动读指针
    if (size > capacity_ - read_pos_)
    {
        size_t first_part = capacity_ - read_pos_;
        memcpy(dst, buffer_.get() + read_pos_, first_part);
        memcpy(dst + first_part, buffer_.get(), size - first_part);
    }
    else
    {
        memcpy(dst, buffer_.get() + read_pos_, size);
    }

    return Status::Ok();
}

size_t RingBuffer::ReadableSize() const
{
    return readable_size_;
}

size_t RingBuffer::WritableSize() const
{
    return capacity_ - readable_size_;
}

size_t RingBuffer::ContiguousWritableSize() const
{
    size_t writable = WritableSize();
    size_t to_end = capacity_ - write_pos_;
    return writable < to_end ? writable : to_end;
}

size_t RingBuffer::ContiguousReadableSize() const
{
    size_t readable = ReadableSize();
    size_t to_end = capacity_ - read_pos_;
    return readable < to_end ? readable : to_end;
}

const char* RingBuffer::ReadPtr() const
{
    return buffer_.get() + read_pos_;
}

char* RingBuffer::WritePtr()
{
    return buffer_.get() + write_pos_;
}

void RingBuffer::Consume(size_t size)
{
    if (size > readable_size_)
    {
        LOG_WARN("RingBuffer::Consume: size exceeds readable size");
        size = readable_size_;
    }

    read_pos_ = (read_pos_ + size) % capacity_;
    readable_size_ -= size;
}

void RingBuffer::Produce(size_t size)
{
    if (size > WritableSize())
    {
        LOG_WARN("RingBuffer::Produce: size exceeds writable size");
        size = WritableSize();
    }

    write_pos_ = (write_pos_ + size) % capacity_;
    readable_size_ += size;
}

size_t RingBuffer::FindChar(char c, size_t max_search_len) const
{
    if (readable_size_ == 0)
    {
        return npos;
    }

    size_t search_len = max_search_len > 0 ? std::min(max_search_len, readable_size_) : readable_size_;
    size_t contiguous = ContiguousReadableSize();

    const char* buf = buffer_.get() + read_pos_;
    const char* found = reinterpret_cast<const char*>(memchr(buf, c, std::min(search_len, contiguous)));
    if (found)
    {
        return found - buf;
    }

    if (search_len > contiguous)
    {
        buf = buffer_.get();
        found = reinterpret_cast<const char*>(memchr(buf, c, search_len - contiguous));
        if (found)
        {
            return contiguous + (found - buf);
        }
    }

    return npos;
}

size_t RingBuffer::FindSubstring(const char* substr, size_t substr_len, size_t max_search_len) const
{
    if (readable_size_ == 0 || !substr || substr_len == 0)
    {
        return npos;
    }

    size_t search_len = max_search_len > 0 ? std::min(max_search_len, readable_size_) : readable_size_;
    if (search_len < substr_len)
    {
        return npos;
    }

    const char* buf = buffer_.get();

    for (size_t i = 0; i <= search_len - substr_len; ++i)
    {
        size_t pos = (read_pos_ + i) % capacity_;
        bool match = true;

        for (size_t j = 0; j < substr_len; ++j)
        {
            size_t char_pos = (pos + j) % capacity_;
            if (buf[char_pos] != substr[j])
            {
                match = false;
                break;
            }
        }

        if (match)
        {
            return i;
        }
    }

    return npos;
}

}  // namespace rtsp_forward
