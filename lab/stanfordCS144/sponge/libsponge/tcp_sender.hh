#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

// !
class RetransmissionTimer {
  private:
    bool _running;
    const unsigned int _init_rto;
    unsigned int _ms_since_running;
    unsigned int _rto;
    unsigned int _consecutive_retransmissions{0};

  public:
    RetransmissionTimer(const unsigned int retx_timeout)
        : _running(false), _init_rto(retx_timeout), _ms_since_running(0), _rto(retx_timeout) {}
    bool timeout(const size_t ms_since_last_tick) {
        if (_ms_since_running + ms_since_last_tick >= _rto) {
            return true;
        }
        _ms_since_running += ms_since_last_tick;
        return false;
    }
    bool running() { return _running; }
    void reset_timer(const size_t window_size) {
        if (!running()) {
            return;  // this line should never be executed
        }
        _ms_since_running = 0;
        ++_consecutive_retransmissions;
        // If the window size is nonzero, Double the value of RTO
        if (window_size > 0) {
            _rto <<= 1;
        }
    }
    void start() {
        _running = true;
        _ms_since_running = 0;
        _rto = _init_rto;
        _consecutive_retransmissions = 0;
    }
    void close() {
        _running = false;
        _consecutive_retransmissions = 0;
    }
    unsigned int consecutive_retransmissions() const { return _consecutive_retransmissions; };
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};
    std::queue<TCPSegment> _segments_track{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;
    RetransmissionTimer _timer;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};
    uint64_t _abs_ackno{0};
    uint16_t _window_size{1};
    uint64_t _bytes_in_flight{0};

    void send_no_empty_segment(TCPSegment &segment);

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
