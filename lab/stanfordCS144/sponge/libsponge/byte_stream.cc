#include "byte_stream.hh"

#include <sstream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity)
    , _bytes_written(0)
    , _bytes_read(0)
    , _buffer{}
    , _buffer_size{0}
    , _end_input(false)
    , _error(false) {}

size_t ByteStream::write(const string &data) {
    size_t count = min(_capacity - _buffer_size, data.length());
    _buffer.append(BufferList(std::move(data.substr(0, count))));
    _buffer_size += count;
    _bytes_written += count;
    return count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t count = min(len, _buffer_size);
    return _buffer.concatenate(count);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t count = min(len, _buffer_size);
    _buffer.remove_prefix(count);
    _bytes_read += count;
    _buffer_size -= count;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
string ByteStream::read(const size_t len) {
    const auto ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _buffer_size; }

bool ByteStream::buffer_empty() const { return _buffer_size == 0; }

bool ByteStream::eof() const { return _end_input && _buffer_size == 0; }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer_size; }
