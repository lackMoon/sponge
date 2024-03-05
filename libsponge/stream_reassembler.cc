#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _unassembled_buffer(capacity, {false, 0}) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    auto seq_end = index + data.length();
    auto first_unassembled_byte = _output.bytes_written();
    auto buffer_end = first_unassembled_byte + _output.remaining_capacity();
    auto end = std::min(seq_end, buffer_end);
    _eof_byte = eof ? std::min(seq_end, _eof_byte) : _eof_byte;
    auto pos = std::max(first_unassembled_byte, index);
    auto buffer_pos = first_unassembled_byte < index ? index - first_unassembled_byte : 0;
    auto seq_pos = first_unassembled_byte < index ? 0 : first_unassembled_byte - index;
    for (; pos < end; pos++) {
        auto idx = (_buffer_start + buffer_pos) % _capacity;
        _buffer_size += _unassembled_buffer[idx].first ^ true;
        _unassembled_buffer[idx] = {true, data[seq_pos]};
        buffer_pos++;
        seq_pos++;
    }
    std::string push_data;
    while (_unassembled_buffer[_buffer_start].first != false) {
        push_data.push_back(_unassembled_buffer[_buffer_start].second);
        _unassembled_buffer[_buffer_start].first = false;
        _buffer_start = (_buffer_start + 1) % _capacity;
        _buffer_size--;
        first_unassembled_byte++;
    }
    _output.write(push_data);
    if (first_unassembled_byte == _eof_byte) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _buffer_size; }

bool StreamReassembler::empty() const { return _output.buffer_empty() && _buffer_size == 0; }