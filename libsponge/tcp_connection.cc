#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPConnection::close() {
    _segments_out = {};
    _active = false;
}

void TCPConnection::flush() {
    auto &sender_queue = _sender.segments_out();
    while (!sender_queue.empty()) {
        auto &seg = sender_queue.front();
        auto ackno = _receiver.ackno();
        if (ackno.has_value()) {
            auto &header = seg.header();
            header.ack = true;
            header.ackno = ackno.value();
            header.win = min(_receiver.window_size(), static_cast<size_t>(numeric_limits<uint16_t>::max()));
        }
        _segments_out.push(seg);
        sender_queue.pop();
    }
}

void TCPConnection::send_rst_segment() {
    TCPSegment seg;
    seg.header().rst = true;
    _segments_out.emplace(std::move(seg));
}

void TCPConnection::fill_window() {
    _sender.fill_window();
    flush();
}

void TCPConnection::reset() {
    _sender.stream_in().set_error();
    inbound_stream().set_error();
    close();
}
size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    auto &header = seg.header();
    if (header.rst) {
        reset();
        return;
    }
    if (header.ack && (_sender.next_seqno_absolute() != 0 || header.syn)) {
        _sender.ack_received(header.ackno, header.win);
    }
    _receiver.segment_received(seg);
    _time_since_last_segment_received = 0;
    if (_receiver.ackno().has_value() &&
        ((seg.length_in_sequence_space() == 0 && header.seqno == _receiver.ackno().value() - 1) ||
         seg.length_in_sequence_space() != 0)) {
        if (_sender.next_seqno_absolute() == 0) {
            connect();
            return;
        }
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
    }
    flush();
    if (inbound_stream().input_ended()) {
        if (!_sender.stream_in().eof()) {
            _linger_after_streams_finish = false;
        } else if (!(_linger_after_streams_finish || _sender.bytes_in_flight())) {
            close();
        }
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    auto write_bytes = _sender.stream_in().write(data);
    fill_window();
    return write_bytes;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    flush();
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        reset();
        send_rst_segment();
    }
    if (_sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && inbound_stream().input_ended()) {
        if (!_linger_after_streams_finish || _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            close();
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    fill_window();
}

void TCPConnection::connect() { fill_window(); }

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            reset();
            send_rst_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
