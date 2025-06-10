#include "Buffer.h"
#include <sys/uio.h> // struct iovec
#include <unistd.h>

// 从 fd 上读取数据 Pooler工作在 LT模式
    // Buffer缓冲区是有大小的, 但是从fd上读数据的时候, 并不知道 tcp数据最终的大小
    // 只有读完才知道, 所以这个缓冲区,  因此 开的太大,太小都不合理
ssize_t Buffer::readFd(int fd, int* savedErrno)
{
    char extrabuf[65536]; // 栈上的内存空间--65536字节= 64K
    struct iovec vec[2];

    const size_t writeable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writeable; // 可写区域大小


    // 如果上面可写区域不够, 就使用栈上的开辟的这个巨大空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf); // 栈上额外空间

    // 并且, 在第二个区域写完后, 再扩容第一个区域, 扩容, 把 第二个里面的内容 放进去, 使得存储更高效 

    // iovcnt的值决定了我们使用多少个 iovec 结构体
    // 当这个缓冲区有足够的空间时，不需要读取到extrabuf中。
    // 当使用extrabuf时，我们最多读取128k-(减)1字节的数据。
    const int iovcnt = (writeable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0)
    {
        *savedErrno = errno; // 保存错误码

    }
    else if (n <= writeable)  // 说明只有第一个区域用了
    {
        writerIndex_ += n; // 更新写索引--- 剩下的可写区域变小了
    }
    else if (n > writeable)
    {
        writerIndex_ = buffer_.size(); // 写索引到末尾, 然后扩容
        append(extrabuf, n - writeable); // 把第二个区域的数据添加到buffer_缓冲区

    }
    return n; // 返回实际读取的字节数
}

// 这个用于 将缓冲区数据 写入fd
// 注意区分, 实际使用, 会有两个buffer类, 读buffer使用上面的函数, 写buffer使用下面的函数
ssize_t Buffer::writeFd(int fd, int* savedErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0)
    {
        *savedErrno = errno; // 保存错误码
    }

    return n;

}
