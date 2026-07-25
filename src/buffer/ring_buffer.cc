#include "ring_buffer.h"

#include <cstring>

#include "util/log.h"

namespace rtsp_server
{

RingBuffer::RingBuffer(size_t capacity) : capacity_(capacity), read_pos_(0), write_pos_(0), readable_size_(0)
{
    buffer_ = std::unique_ptr<char[]>(new char[capacity]);
    LOG_DEBUG("RingBuffer created, capacity=%zu", capacity);
}

RingBuffer::~RingBuffer()
{
    LOG_DEBUG("RingBuffer destroyed");
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

    // 如果写入的数据不超过写指针到缓冲区末尾的距离
    if (size <= capacity_ - write_pos_)
    {
        memcpy(buffer_.get() + write_pos_, src, size);
        write_pos_ += size;
    }
    else
    {
        // 需要环绕写入
        size_t first_part = capacity_ - write_pos_;
        memcpy(buffer_.get() + write_pos_, src, first_part);
        memcpy(buffer_.get(), src + first_part, size - first_part);
        write_pos_ = size - first_part;
    }

    readable_size_ += size;
    return Status::Ok();
}

Status RingBuffer::Read(void* data, size_t size)
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

    // 如果读取的数据不超过读指针到缓冲区末尾的距离
    if (size <= capacity_ - read_pos_)
    {
        memcpy(dst, buffer_.get() + read_pos_, size);
        read_pos_ += size;
    }
    else
    {
        // 需要环绕读取
        size_t first_part = capacity_ - read_pos_;
        memcpy(dst, buffer_.get() + read_pos_, first_part);
        memcpy(dst + first_part, buffer_.get(), size - first_part);
        read_pos_ = size - first_part;
    }

    readable_size_ -= size;
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

    // 如果读取的数据不超过读指针到缓冲区末尾的距离
    if (size <= capacity_ - read_pos_)
    {
        memcpy(dst, buffer_.get() + read_pos_, size);
    }
    else
    {
        // 需要环绕读取
        size_t first_part = capacity_ - read_pos_;
        memcpy(dst, buffer_.get() + read_pos_, first_part);
        memcpy(dst + first_part, buffer_.get(), size - first_part);
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

size_t RingBuffer::Capacity() const
{
    return capacity_;
}

void RingBuffer::Clear()
{
    read_pos_ = 0;
    write_pos_ = 0;
    readable_size_ = 0;
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

}  // namespace rtsp_server
