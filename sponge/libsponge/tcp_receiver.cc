#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    uint64_t abseq = 0;
    if(listen){
        if(!seg.header().syn) return;
         listen = false;
         isn.setwrap(seg.header().seqno.raw_value());
    }
    WrappingInt32 seq = seg.header().seqno;
    abseq = unwrap(seq,isn,stream_out().bytes_written() + 1);
    uint64_t stream_idx = abseq - 1 + (seg.header().syn);
    _reassembler.push_substring(seg.payload().copy(),stream_idx,seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(listen) return {};
    else{
       uint64_t ack = stream_out().bytes_written() + 1;
       if(stream_out().input_ended()) return wrap(ack + 1,isn);
       else return wrap(ack,isn);
    }
}

size_t TCPReceiver::window_size() const { 
    return stream_out().remaining_capacity();
}
