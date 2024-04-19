#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : capacity_(capacity), conatiner_(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t len = data.length();
    auto write_bytes = min(len, remaining_capacity());
    for (size_t i = 0; i < write_bytes; i++) {
        conatiner_[tail_] = data[i];
        tail_ = (tail_ + 1) % capacity_;
    }
    writes_ += write_bytes;
    size_ += write_bytes;
    return write_bytes;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::string ret_str;
    auto peek_bytes = min(size_, len);
    auto index = head_;
    ret_str.reserve(peek_bytes);
    for (size_t i = 0; i < peek_bytes; i++) {
        ret_str.push_back(conatiner_.at(index));
        index = (index + 1) % capacity_;
    }
    return ret_str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    auto pop_byte = min(size_, len);
    head_ = (head_ + pop_byte) % capacity_;
    size_ -= pop_byte;
    reads_ += pop_byte;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto ret_str = std::move(peek_output(len));
    auto read_bytes = ret_str.length();
    head_ = (head_ + read_bytes) % capacity_;
    size_ -= read_bytes;
    reads_ += read_bytes;
    return ret_str;
}

void ByteStream::end_input() { is_ended_ = true; }

bool ByteStream::input_ended() const { return is_ended_; }

size_t ByteStream::buffer_size() const { return size_; }

bool ByteStream::buffer_empty() const { return size_ == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return writes_; }

size_t ByteStream::bytes_read() const { return reads_; }

size_t ByteStream::remaining_capacity() const { return capacity_ - size_; }