#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _unassemble_buffer(), 
    _next_pos(0), _unassembled_bytes(0), _eof(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index > _next_pos + _capacity) {
        return;
    }
    insert_pair(data, index);
    if (index <= _next_pos) {
        write_output();
    }

    if (eof) {
        _eof = true;
    }
    if (_eof && empty()) {
        _output.end_input();
    }
}

void StreamReassembler::insert_pair(const string &data, const size_t index) {
    size_t data_length = data.length();
    size_t delta = 0;
    if (index < _next_pos) {
        if (index + data.length() <= _next_pos) {
            return;
        }
        data_length = index + data.length() - _next_pos;
        delta = data.length() - data_length;
    }
    Node node{index + delta, data_length, data.substr(delta)};
    _unassembled_bytes += node.length;

    auto it = _unassemble_buffer.lower_bound(node);
    int merged_bytes;
    while (it != _unassemble_buffer.end() && (merged_bytes = node.merge(*it)) >= 0) {
        _unassemble_buffer.erase(it);
        _unassembled_bytes -= merged_bytes;
        it = _unassemble_buffer.lower_bound(node);
    }

    while (!_unassemble_buffer.empty()) {
        it = _unassemble_buffer.lower_bound(node);
        if (it == _unassemble_buffer.begin()) {
            break;
        }
        --it;
        if ((merged_bytes = node.merge(*it)) < 0) {
            break;
        }
        _unassemble_buffer.erase(it);
        _unassembled_bytes -= merged_bytes;
    }
    _unassemble_buffer.insert(node);
}

void StreamReassembler::write_output() {
    if (_unassemble_buffer.empty()) {
        return;
    }
    auto it = _unassemble_buffer.begin();
    if (it->index == _next_pos) {
        size_t write_bytes = _output.write(it->data);
        _unassembled_bytes -= write_bytes;
        _next_pos += write_bytes;
        _unassemble_buffer.erase(it);
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }
