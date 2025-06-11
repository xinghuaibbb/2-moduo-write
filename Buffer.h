#pragma once

#include <vector>
#include <string>
#include <algorithm> // std::copy



// 网络库底层的 缓冲区类型定义
class Buffer
{

public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + kInitialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }


    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    // 返回缓冲区中 可读数据的起始地址
    const char* peek() const
    {
        return begin() + readerIndex_;
    }


    /*
- **`retrieve(size_t len)`**: 读取指定长度的数据。 并 更新 可读区域
- **`retrieveAll()`**: 清空缓冲区。  就是 读完了, 重置区域
- **`retrieveAllAsString()`**: 将所有可读数据转换为字符串。
- **`retrieveAsString(size_t len)`**: 将指定长度的数据转换为字符串。
    */

    // onMessage 处理网络消息时, 把数据放到buffer里面
    //  buffer(char) --> string 类型  序列化,反序列化 都是string 类型
    void retrieve(size_t len)  
    {
        if (len < readableBytes())
        {
            // 只读了一部分数据, 需要移动读索引
            readerIndex_ += len;   // readerIndex_ += len ==> writerIndex_  数据 还没读
        }
        else  // len == readableBytes()  读完了
        {
            retrieveAll();
        }

    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend; // 重置读写索引
    }

    // 把onMessage 函数上报的buffer数据, 转成 string 返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); // 调用可读取的数据的长度
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len);  // 更新区域, 如果 len == readableBytes() 则重置 readerIndex_ 和 writerIndex_
        return result;
    }

    // buffer_.size() - wrierIndex_ 可写区域大小
    // 如果不够呢?  扩容
    // 函数作用---保证 读写空间
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            // 扩容
            makeSpace(len);
        }
    }

    // 把[data, data+len] 内存上的数据, 添加到writeable缓冲区中
    void append(const char* data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    ssize_t readFd(int fd, int* savedErrno);
    ssize_t writeFd(int fd, int* savedErrno);

private:
    char* begin()
    {
        return &*buffer_.begin();
    }

    const char* begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else  // 未读数据 往前挪
        {
            size_t readable = readableBytes(); // 先获得 未读区域大小
            std::copy(begin() + readerIndex_,
                begin() + writerIndex_,
                begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }


    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};