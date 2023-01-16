#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const auto& header = seg.header();
    if (!_syn) {
        // discard all the packages before LISTEN state;
        if (!header.syn) {
            return;
        }
        _syn = true;
        _isn = header.seqno;
    }

    uint64_t checkpoint = _reassembler.stream_out().bytes_written() + 1;
    uint64_t abs_seqno = unwrap(header.seqno, _isn, checkpoint);
    uint64_t stream_index = abs_seqno - 1 + (header.syn ? 1 : 0);
    _reassembler.push_substring(seg.payload().copy(), stream_index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_syn) {
        return nullopt;
    }
    uint64_t bytes_written = _reassembler.stream_out().bytes_written() + 1;
    return _isn + static_cast<uint32_t>(bytes_written) + (_reassembler.stream_out().input_ended() ? 1 : 0); 
}

size_t TCPReceiver::window_size() const { 
    return _capacity - _reassembler.stream_out().buffer_size(); 
}
