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
    : _capacity(capacity), _bytes_written(0), _bytes_read(0), _buffer{}, _end_input(false), _error(false) {
    _buffer.clear();
}

size_t ByteStream::write(const string &data) {
    size_t count = min(_capacity - _buffer.size(), data.length());
    for (size_t i = 0; i < count; i++) {
        _buffer.push_back(data[i]);
    }
    _bytes_written += count;
    return count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    // ostringstream oss;
    string out;
    size_t end_pos = min(len, _buffer.size());
    for (size_t i = 0; i < end_pos; ++i) {
        // oss << _buffer[i];
        out += _buffer[i];
    }
    return out;
    // return oss.str();
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len > _buffer.size()) {
        set_error();
        return;
    }
    for (size_t i = 0; i < len; ++i) {
        _buffer.pop_front();
    }
    _bytes_read += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
string ByteStream::read(const size_t len) {
    if (len > _buffer.size()) {
        set_error();
        return "";
    }
    ostringstream oss;
    // string out = "";
    for (size_t i = 0; i < len; ++i) {
        oss << _buffer.front();
        // out += _buffer.front();
        _buffer.pop_front();
    }
    _bytes_read += len;
    return oss.str();
    // return out;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return _end_input && _buffer.empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
