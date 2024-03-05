#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto &seg_header = seg.header();
    auto seg_body = seg.payload().copy();
    auto seq_no = seg_header.seqno;
    if (seg_header.syn) {
        _isn = seq_no;
        _idx_offset = (seg_body.size() == 0 && !seg_header.fin);
    }
    if (_isn.has_value()) {
        auto check_point = _reassembler.stream_out().bytes_written();
        auto idx = unwrap(seq_no, _isn.value(), check_point) - _idx_offset;
        _reassembler.push_substring(seg_body, idx, seg_header.fin);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    return _isn.has_value() ? optional<WrappingInt32>{wrap(_reassembler.stream_out().bytes_written() +
                                                               _reassembler.stream_out().input_ended() + 1,
                                                           _isn.value())}
                            : nullopt;
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
