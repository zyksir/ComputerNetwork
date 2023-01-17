#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _timer(retx_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    if (_next_seqno == 0) {  // Status: CLOSED
        TCPSegment segment;
        segment.header().syn = true;
        send_no_empty_segment(segment);
    } else if (_next_seqno == _bytes_in_flight) {  // Status: SYN_SENT
        return;
    }

    size_t window_size = _window_size == 0 ? 1 : _window_size;
    while (window_size > _next_seqno - _max_ackno) {
        size_t remain = window_size - (_next_seqno - _max_ackno);
        if (!_stream.eof()) {
            size_t payload_size = std::min(remain, TCPConfig::MAX_PAYLOAD_SIZE);
            TCPSegment segment;
            segment.payload() = Buffer(_stream.read(payload_size));
            // if we met eof and there is space in the window
            if (_stream.eof() && (remain - segment.length_in_sequence_space() > 0)) {
                segment.header().fin = true;
            }
            if (segment.length_in_sequence_space() == 0) {
                return;
            }
            send_no_empty_segment(segment);
        } else {
            if (_next_seqno < _stream.bytes_written() + 2) {  // Status: SYN_ACKED
                TCPSegment segment;
                segment.header().fin = true;
                send_no_empty_segment(segment);
            }
            return;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // step1: update _segments_track and window_size
    uint64_t abs_ackno = unwrap(ackno, _isn, _max_ackno);
    // illegal abs_ackno
    if (abs_ackno > _next_seqno) {
        return;
    }
    _window_size = static_cast<size_t>(window_size);
    if (abs_ackno <= _max_ackno) {
        return;
    }

    _max_ackno = abs_ackno;
    // remove fully-acknowledged segments
    while (!_segments_track.empty()) {
        const auto segment = _segments_track.front();
        uint64_t max_seg_ackno = unwrap(segment.header().seqno, _isn, _max_ackno) + segment.length_in_sequence_space();
        if (abs_ackno < max_seg_ackno) {
            break;
        }
        _segments_track.pop();
        _bytes_in_flight -= segment.length_in_sequence_space();
    }

    // step 2: Set the RTO back to its “initial value.”
    _timer.reset_rto();
    // step 3: If there is any outstanding data, restart the retransmission timer
    // step 4: reset consecutive retransmissions to zero
    if (!_segments_track.empty()) {
        _timer.start();
    } else {
        _timer.close();
    }

    // The TCPSender should ﬁll the window again if new space has opened up.
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // do following steps only if the retransmission timer has expired
    if (!_timer.running() || !_timer.timeout(ms_since_last_tick)) {
        return;
    }

    // step 1: Retransmit the earliest segment
    if (_segments_track.empty()) {
        _timer.close();
        return;
    }
    auto segment = _segments_track.front();
    _segments_out.push(_segments_track.front());

    // step 2: Reset the retransmission timer
    //  2.1 double rto if window_size is non zero
    //  2.2 increase _consecutive_retransmissions
    _timer.reset_timer(_window_size);
}

unsigned int TCPSender::consecutive_retransmissions() const { return _timer.consecutive_retransmissions(); }

void TCPSender::send_empty_segment() {
    TCPSegment empty_segment;
    empty_segment.header().seqno = next_seqno();
    _segments_out.push(empty_segment);
}

void TCPSender::send_no_empty_segment(TCPSegment &segment) {
    segment.header().seqno = next_seqno();
    _next_seqno += segment.length_in_sequence_space();
    _bytes_in_flight += segment.length_in_sequence_space();
    _segments_out.push(segment);
    _segments_track.push(segment);
    if (!_timer.running()) {
        _timer.start();
    }
}
