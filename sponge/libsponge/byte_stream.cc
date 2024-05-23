#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... ) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):capa(capacity){}

size_t ByteStream::write(const string &data) {
   if(!end_in)
   {
    string stream = data;
    if(stream.size()>remaining_capacity()) 
      stream = stream.substr(0,remaining_capacity());
    //this->size += stream.size();
    room += stream;
    sum_w += stream.size();
    return stream.size();
   }
   else return 0;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
   size_t length = len > room.size()? room.size():len;
   return room.substr(0,length);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t length = len > room.size()? room.size():len;
    room = room.substr(length);
    //size -= length;
    sum_r += length;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    size_t length = len > room.size()? room.size():len;
    string data = peek_output(length);
    pop_output(length);
    return data;
}

void ByteStream::end_input() { end_in = true; }

bool ByteStream::input_ended() const { return end_in; }

size_t ByteStream::buffer_size() const { return room.size(); }

bool ByteStream::buffer_empty() const { return room.size()==0; }

bool ByteStream::eof() const { return end_in && buffer_empty(); }

size_t ByteStream::bytes_written() const { return sum_w; }

size_t ByteStream::bytes_read() const { return sum_r; }

size_t ByteStream::remaining_capacity() const { return capa-room.size(); }

