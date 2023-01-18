#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + static_cast<uint32_t>(n); }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // static constexpr uint32_t neg_threshold = (1u << 31);
    // static constexpr uint64_t uint32_max = (1ul << 32);
    constexpr uint32_t neg_threshold = (1u << 31);
    constexpr uint64_t uint32_max = (1ul << 32);
    uint32_t offset = n - wrap(checkpoint, isn);
    uint64_t ret = checkpoint + static_cast<uint64_t>(offset);
    // 有可能此时 ret 对应的值在 checkpoint 后面
    // 这种情况下，offset 是负数，我们判断其符号位即可
    // 此时 ret 应该减去 1<<32
    // if ((offset & neg_threshold) && (ret>>32))
    if ((offset & neg_threshold) && (ret >= uint32_max))
        ret -= uint32_max;
    return ret;
}
