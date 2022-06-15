#ifndef __FLUENT_NETWORK_BUFFER_H__
#define __FLUENT_NETWORK_BUFFER_H__
#include <sys/uio.h>
#include <unistd.h>
#include <bits/stdc++.h>
#include "Socket.h"
#include "../utils/Algorithms.h"
namespace fluent {

// TODO use small_vector
class Buffer {
public:
    Buffer(ssize_t size = 128);

    ssize_t size() const { return _capacity; }
    ssize_t unread() const { return _w - _r; }
    ssize_t unwrite() const { return _capacity - _w; }
    ssize_t available() const { return _capacity - unread(); }

    void clear() { _r = _w = 0; }
    void reuseIfPossible();
    void gc();
    void gc(ssize_t hint);
    void expand() { _buf.resize(_capacity <<= 1); }
    void expandTo(ssize_t size) { _buf.resize(_capacity = size); }

    void hasRead(ssize_t n) { _r += n; reuseIfPossible(); }
    void hasWritten(ssize_t n) { _w += n; }

    const char* readBuffer() const { return _buf.data() + _r; }
    char* readBuffer() { return _buf.data() + _r; }
    const char* writeBuffer() const { return _buf.data() + _w; }
    char* writeBuffer() { return _buf.data() + _w; }
    const char* end() const { return _buf.data() + _capacity; }

    void append(const char *data, ssize_t size);
    template <typename T, typename = std::enable_if_t<!std::is_pointer<T>::value>>
    void append(const T &data) { append((const char *)(&data), sizeof(T)); }
    template <typename T, size_t N>
    void append(const T (&data)[N]) { append(data, sizeof(T) * N); }

    ssize_t readFrom(const Socket &socket) { return readFrom(socket.fd()); }
    ssize_t readFrom(int fd);
    ssize_t writeTo(int fd);

private:
    std::vector<char> _buf;
    ssize_t _capacity;
    ssize_t _r, _w; // _r <= _w
};

inline Buffer::Buffer(ssize_t size)
    : _capacity(size),
      _buf(size),
      _r(0), _w(0) {}

inline void Buffer::reuseIfPossible() {
    if(_r == _w) {
        clear();
        return;
    }
    ssize_t bufferRest = unread();
    if(bufferRest < (_capacity >> 4) && _r > (_capacity >> 4)) {
        gc();
    }
}

inline void Buffer::gc() {
    if(_r > 0) {
        // move [_r, _w) to front
        memmove(_buf.data(), _buf.data() + _r, _w = unread());
        _r = 0;
    }
}

inline void Buffer::gc(ssize_t hint) {
    if(hint <= unwrite()) return;
    gc();
    // still cannot store
    if(hint > available()) {
        ssize_t appendSize =  hint - available();
        ssize_t expectSize = _buf.size() + appendSize;
        expandTo(/*_capacity = */roundToPowerOfTwo(expectSize));
    }
}

inline void Buffer::append(const char *data, ssize_t size) {
    gc(size);
    memcpy(writeBuffer(), data, size);
    hasWritten(size);
}

inline ssize_t Buffer::readFrom(int fd) {
    char localBuffer[1<<16];
    iovec vec[2];
    ssize_t bufferLimit = unwrite();
    vec[0].iov_base = writeBuffer();
    vec[0].iov_len = bufferLimit;
    vec[1].iov_base = localBuffer;
    vec[1].iov_len = sizeof(localBuffer);
    ssize_t n = ::readv(fd, vec, 2);
    if(n > 0) {
        if(n <= bufferLimit) {
            hasWritten(n);
        } else {
            hasWritten(bufferLimit);
            ssize_t localSize = n - bufferLimit;
            append(localBuffer, localSize);
        }
    }
    // n < 0 error
    return n;
}

inline ssize_t Buffer::writeTo(int fd) {
    ssize_t n = ::write(fd, readBuffer(), unread());
    if(n < 0) {
        switch(errno) {
            case EAGAIN:
            case EINTR:
                n = 0;
            break;
            default:
                throw WriteException(errno);
            break;
        }
    }
    hasRead(n);
    return n;
}

inline std::ostream& operator << (std::ostream &os, Buffer &buf) {
    auto iter = buf.readBuffer();
    const auto bound = buf.writeBuffer();
    while(iter != bound) os << *iter++;
    return os;
}

} // fluent
#endif