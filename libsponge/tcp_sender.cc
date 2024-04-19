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
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _send_base; }

void TCPSender::fill_window() {
    while (is_connected) {
        TCPSegment seg;
        bool is_syn = _next_seqno == 0;
        auto size = (_window_size + !_window_size) - bytes_in_flight();
        if (size == 0) {  // window is full
            return;
        }
        auto payload_size = min(min(size - is_syn, _stream.buffer_size()), TCPConfig::MAX_PAYLOAD_SIZE);
        seg.payload() = _stream.read(payload_size);
        auto &header = seg.header();
        header.seqno = next_seqno();
        header.syn = is_syn;
        header.fin = payload_size < size && _stream.eof();
        is_connected = !header.fin;
        if (seg.length_in_sequence_space() == 0) {  // empty sequence
            return;
        }
        _segments_out.push(seg);
        _segments_unack.push({_next_seqno, seg});
        _timer.turn(true);
        _next_seqno += seg.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t abs_ack = unwrap(ackno, _isn, _send_base);
    if (abs_ack > _send_base && abs_ack <= _next_seqno) {
        _send_base = abs_ack;
        while (!_segments_unack.empty()) {
            auto &out_seg = _segments_unack.front();
            auto seg_byte = out_seg.first + out_seg.second.length_in_sequence_space();
            if (seg_byte > _send_base) {
                break;
            }
            _segments_unack.pop();
        }
        _timer.reset(_initial_retransmission_timeout);
        bool is_restart = !_segments_unack.empty();
        _timer.turn(is_restart, true);
        _consecutive_retransmissions = 0;
    }
    _window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.pass(ms_since_last_tick);
    if (_timer.is_expired()) {
        auto &seg = _segments_unack.front().second;
        _segments_out.push(seg);
        if (_window_size != 0) {
            _consecutive_retransmissions++;
            _timer.reset();
        }
        _timer.turn(true, true);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
