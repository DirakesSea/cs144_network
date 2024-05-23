#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return time_since_last_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    
    bool need_send_ack = seg.length_in_sequence_space();
    // you code here.
    //你需要考虑到ACK包、RST包等多种情况
    time_since_last_received = 0;

    if(seg.header().rst){
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _linger_after_streams_finish = false;
        _is_active = false;
        return;
    }else{ 
        _receiver.segment_received(seg);
       if(seg.header().ack){
        _sender.ack_received(seg.header().ackno,seg.header().win);
        if(need_send_ack && !_sender.segments_out().empty()) need_send_ack = false;
       } 
    }

    //状态变化(按照个人的情况可进行修改)
    // 如果是 LISEN 到了 SYN
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        // 此时肯定是第一次调用 fill_window，因此会发送 SYN + ACK
        connect();
        return;
    }

    // 判断 TCP 断开连接时是否时需要等待
    // CLOSE_WAIT
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
        _linger_after_streams_finish = false;

    // 如果到了准备断开连接的时候。服务器端先断
    // CLOSED
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        _is_active = false;
        return;
    }

    // 如果收到的数据包里没有任何数据，则这个数据包可能只是为了 keep-alive
    if (need_send_ack)
        _sender.send_empty_segment();
    send_queue();
}

void TCPConnection::send_queue(){
   while(!_sender.segments_out().empty()){
    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    if(_receiver.ackno().has_value()){
     seg.header().ack = true;
     seg.header().ackno = _receiver.ackno().value();
     seg.header().win  = _receiver.window_size();
    }
   _segments_out.push(seg);
   }
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    size_t num = _sender.stream_in().write(data);
    _sender.fill_window();
    send_queue();
    return num;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
     time_since_last_received += ms_since_last_tick;
     _sender.tick(ms_since_last_tick);
     if(_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS){
       _sender.segments_out().pop();
       _sender.stream_in().set_error();
       _receiver.stream_out().set_error();
       _linger_after_streams_finish = false;
       _is_active = false;
       TCPSegment seg;
       seg.header().rst = true;
       _segments_out.push(seg);
       return;
     }  
     send_queue();
     if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish 
        && time_since_last_received >= 10*_cfg.rt_timeout){
          _is_active = false;
          _linger_after_streams_finish = false;  
        }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_queue();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _is_active = true;
    send_queue();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _linger_after_streams_finish = false;
            _is_active = false;
            TCPSegment seg;
            seg.header().rst = true;
            _segments_out.push(seg);
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
