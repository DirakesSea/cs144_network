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
//! \param[in] retxrto the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retxrto, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retxrto}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _byte_in_flight; }

void TCPSender::fill_window() {
  uint64_t window = (win == 0)? 1 : win;
  while(_byte_in_flight < window){
    TCPSegment seg;
    if(!syn){
      syn = true;
      seg.header().syn = true;
    }
    if(_segments_flight.empty()){
      rto = _initial_retransmission_timeout;
      timer = 0;
    }
    seg.header().seqno = wrap(_next_seqno, _isn);
    uint64_t one_load = TCPConfig::MAX_PAYLOAD_SIZE <= (window - _byte_in_flight - seg.header().syn) ? 
         TCPConfig::MAX_PAYLOAD_SIZE : (window - _byte_in_flight - seg.header().syn);
    string load = _stream.read(one_load);
    if(!fin && _stream.eof() && load.size() + _byte_in_flight < window){
      fin = true;
      seg.header().fin = true;
    }  
    seg.payload() = Buffer(move(load));   
    if(seg.length_in_sequence_space() == 0) break;
    _segments_out.push(seg);
    _byte_in_flight += seg.length_in_sequence_space();
    _next_seqno += seg.length_in_sequence_space();
    _segments_flight.push(seg);
    if(seg.header().fin) break;
  }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    uint64_t ack_abseq = unwrap(ackno,_isn,_next_seqno);   
    if(ack_abseq > _next_seqno) return;
    _ack_seqno = ack_abseq;
    while(!_segments_flight.empty()){
       TCPSegment seg = _segments_flight.front();
       if(unwrap(seg.header().seqno,_isn,_next_seqno) + seg.length_in_sequence_space() <= _ack_seqno){
          _segments_flight.pop();
          _byte_in_flight -= seg.length_in_sequence_space();
          rto = _initial_retransmission_timeout;
          timer = 0;
       } else break;
    }
    _consecutive_retransmissions = 0;
    win = window_size;
    fill_window(); 
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    timer += ms_since_last_tick;
    if(timer >= rto && !_segments_flight.empty()){
      _segments_out.push(_segments_flight.front());
      _consecutive_retransmissions++;
      timer = 0;
      if(win > 0) rto *= 2;
    }
}


unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno,_isn);
    _segments_out.push(seg);
}
