#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <set>
#include <string>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    class Node {
      public:
        Node(size_t _index = 0, size_t _length = 0, std::string _data = "")
            : index(_index), length(_length), data(_data) {}
        size_t index;
        size_t length;
        std::string data;
        bool operator<(const Node &rhs) const { return index < rhs.index; }
        int merge(const Node &rhs) {
            if (index < rhs.index) {
                /* cannot be merged
                 *            [ rhs ]
                 * [ this ]
                 */
                if (index + length < rhs.index) {
                    return -1;
                }
                /* don't need to merge
                 *     [    rhs    ]
                 * [       this       ]
                 */
                if (index + length >= rhs.index + rhs.length) {
                    return rhs.length;
                }
                /* append end of rhs.data merge
                 *     [    rhs    ]
                 * [  this  ]
                 */
                int merged_bytes = index + length - rhs.index;
                data = data + rhs.data.substr(index + length - rhs.index);
                length = rhs.index + rhs.length - index;
                return merged_bytes;
            }
            /* cannot be merged
             *            [ this ]
             * [ rhs ]
             */
            if (rhs.index + rhs.length < index) {
                return -1;
            }

            /* don't need to merge
             *     [    this    ]
             * [       rhs       ]
             */
            if (rhs.index + rhs.length >= index + length) {
                int merged_bytes = length;
                index = rhs.index;
                length = rhs.length;
                data = rhs.data;
                return merged_bytes;
            }
            /* append end of rhs.data merge
             *     [    this    ]
             * [  rhs  ]
             */
            int merged_bytes = rhs.index + rhs.length - index;
            data = rhs.data + data.substr(rhs.index + rhs.length - index);
            length = index + length - rhs.index;
            index = rhs.index;
            return merged_bytes;
        }
    };

    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes

    std::set<Node> _unassemble_buffer;
    size_t _next_pos;
    size_t _unassembled_bytes;
    bool _eof;

    void insert_pair(const std::string &data, const size_t index);
    void write_output();

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
