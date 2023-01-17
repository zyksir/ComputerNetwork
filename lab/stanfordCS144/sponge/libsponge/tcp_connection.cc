#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPConnection::send_rst_segment() {
    _sender.send_empty_segment();
    TCPSegment rst_segment = _sender.segments_out().front();
    _sender.segments_out().pop();
    rst_segment.header().rst = true;
    auto ackno = _receiver.ackno();
    if (ackno) {
        rst_segment.header().ackno = ackno.value();
        rst_segment.header().ack = true;
    }
    rst_segment.header().win = static_cast<uint16_t>(_receiver.window_size());
    _segments_out.push(rst_segment);
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

bool TCPConnection::send_segments() {
    bool is_send = false;
    while (!_sender.segments_out().empty()) {
        is_send = true;
        auto segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        // TCPSender has set: seqno, syn, payload, fin
        // TCPConnection should set: ack, window_size
        auto ackno = _receiver.ackno();
        if (ackno.has_value()) {
            segment.header().ackno = ackno.value();
            segment.header().ack = true;
        }
        size_t window_size = _receiver.window_size();
        if (window_size > std::numeric_limits<uint16_t>::max()) {
            window_size = std::numeric_limits<uint16_t>::max();
        }
        segment.header().win = static_cast<uint16_t>(window_size);
        _segments_out.push(segment);
    }
    return is_send;
}

void TCPConnection::segment_received(const TCPSegment &segment) { 
    _time_since_last_segment_received = 0;
    // Step1: if RST is set,
    //      1.1: sets both the inbound and outbound streams to the error state
    //      1.2: [TODO] kills the connection permanently.
    if (segment.header().rst) {
        passive_close();
        return;
    }

    // Step2: send the segment to TCPReceiver
    _receiver.segment_received(segment);

    // Step3: if ack is set, tell TCPSender ackno and window_size
    // size_t current_segment_out_length = _sender.segments_out().size();
    if (segment.header().ack) {
        _sender.ack_received(segment.header().ackno, segment.header().win);
        send_segments();
    }

    // Step4: if the incoming segment occupied any seqno, we need to send at least one segment
    if (segment.length_in_sequence_space() > 0) {
        _sender.fill_window();
        bool is_send = send_segments();
        if (!is_send) {
            _sender.send_empty_segment();
            send_segments();
        }
    }
    test_end();
}

bool TCPConnection::active() const { 
    return _active; 
}

size_t TCPConnection::write(const string &data) {
    if (!data.size()) return 0;
    size_t sz = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segments();
    return sz;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    // Step1: tell the TCPSender about the passage of time
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    // Step2: abort the connection if try too many times
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        passive_close();
        send_rst_segment();
        return;
    }

    send_segments();

    // Step3: end the connection cleanly
    test_end();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
}

void TCPConnection::connect() {
    if (_sender.next_seqno_absolute() != 0)
        return;
    _sender.fill_window();
    send_segments();
    _active = true;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // need to send a RST segment to the peer
            passive_close();
            send_rst_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// prereqs#1 : The inbound stream has been fully assembled and has ended.
bool TCPConnection::check_inbound_ended() {
    return _receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended();
}

// prereqs2 : The outbound stream has been ended by the local application and fully sent (including
// the fact that it ended, i.e. a segment with fin ) to the remote peer.
// prereqs3 : The outbound stream has been fully acknowledged by the remote peer.
bool TCPConnection::check_outbound_ended() {
    return _sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
           _sender.bytes_in_flight() == 0;
}

void TCPConnection::passive_close() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::test_end() {
    // check if need to linger
    // which means check whether it was the first one to end
    if (check_inbound_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    if (check_inbound_ended() && check_outbound_ended()) {
        if (!_linger_after_streams_finish) {
            _active = false;
        } else if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
}