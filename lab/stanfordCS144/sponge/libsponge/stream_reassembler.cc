#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _unassemble_buffer(), _next_pos(0), _unassembled_bytes(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    insert_pair(data, index);
    write_output();

    if (eof) {
        _eof_index = index + data.length();
        _eof_appear_sign = true;
    }
    if (_eof_appear_sign && _next_pos >= _eof_index) {
        _output.end_input();
    }
}

void StreamReassembler::insert_pair(const string &data, const size_t index) {
    size_t new_index = index;
    // merge with string ahead of it
    auto pre_iter = _unassemble_buffer.upper_bound(index);
    if (pre_iter != _unassemble_buffer.begin())
        pre_iter--;
    if (pre_iter != _unassemble_buffer.end() && pre_iter->first <= index) {
        const size_t pre_idx = pre_iter->first;
        if (index < pre_idx + pre_iter->second.size())
            new_index = pre_idx + pre_iter->second.size();
    } else if (index < _next_pos) {
        new_index = _next_pos;
    }

    // this string is contained by pre string
    if (new_index >= index + data.length()) {
        return;
    }

    // discard chars if they are out of capacity
    size_t first_unacceptable_idx = _next_pos + _capacity - _output.buffer_size();
    if (first_unacceptable_idx <= new_index)
        return;

    // merge with string post of it
    size_t new_length = data.length() - (new_index - index);
    auto post_iter = _unassemble_buffer.upper_bound(index);
    while (post_iter != _unassemble_buffer.end() && new_index <= post_iter->first) {
        const size_t data_end_pos = new_index + new_length;
        // if they don't intersect with each other
        if (post_iter->first >= data_end_pos) {
            break;
        }
        // if post string is partially covered by this string
        if (post_iter->first + post_iter->second.length() > data_end_pos) {
            new_length = post_iter->first - new_index;
            break;
        }
        // if post string is fully covered by this string
        _unassembled_bytes -= post_iter->second.size();
        post_iter = _unassemble_buffer.erase(post_iter);
    }

    // we don't need to insert
    if (new_length == 0) {
        return;
    }

    // discard out of memory chars
    new_length = min(first_unacceptable_idx - new_index, new_length);

    string new_data = data.substr(new_index - index, new_length);

    // optimize 1: if we can write this data we don't insert into map
    if (new_index == _next_pos) {
        const size_t write_bytes = _output.write(std::move(new_data));
        _next_pos += write_bytes;
        if (write_bytes == new_data.length()) {
            return;
        }
        new_index += write_bytes;
        new_data = new_data.substr(write_bytes);
    }
    // optimize 1 end

    _unassemble_buffer.emplace(new_index, std::move(new_data));
    _unassembled_bytes += new_length;
}

void StreamReassembler::write_output() {
    auto it = _unassemble_buffer.begin();
    while (!_unassemble_buffer.empty() && it->first == _next_pos && _output.remaining_capacity() > 0) {
        const size_t write_bytes = _output.write(std::move(it->second));
        _unassembled_bytes -= write_bytes;
        _next_pos += write_bytes;
        if (write_bytes < it->second.length()) {
            if (write_bytes == 0) {
                return;
            }
            _unassemble_buffer.emplace(_next_pos, std::move(it->second.substr(write_bytes)));
        }
        _unassemble_buffer.erase(it);
        it = _unassemble_buffer.begin();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
