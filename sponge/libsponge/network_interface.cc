#include "network_interface.hh"
#include <ctime>
#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be 
//   another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    if(arp_tbl.find(next_hop_ip) != arp_tbl.end())
    {
        EthernetFrame frame;
        frame.header().dst = arp_tbl[next_hop_ip].first;
        frame.header().src = this->_ethernet_address;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    }else{
       if(timer.find(next_hop_ip) == timer.end()){
        EthernetFrame request;
        request.header().src = this->_ethernet_address;
        request.header().dst = ETHERNET_BROADCAST;
        request.header().type = EthernetHeader::TYPE_ARP;
        ARPMessage arpreq;
        arpreq.opcode = arpreq.OPCODE_REQUEST;
        arpreq.sender_ethernet_address = this->_ethernet_address;
        arpreq.sender_ip_address = this->_ip_address.ipv4_numeric();
        arpreq.target_ip_address = next_hop_ip;
        arpreq.target_ethernet_address = {};
        request.payload() = arpreq.serialize();
        _frames_out.push(request);
        timer[next_hop_ip] = 0;
       }
       _Datagram.push_back({next_hop_ip,dgram});
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst != this->_ethernet_address && frame.header().dst != ETHERNET_BROADCAST) return {};
    if(frame.header().type == EthernetHeader::TYPE_IPv4)
    {
      InternetDatagram dgram;
      if(dgram.parse(frame.payload()) == ParseResult::NoError)  return dgram;
    }else if(frame.header().type == EthernetHeader::TYPE_ARP)
    {
      ARPMessage arpm;
      if(arpm.parse(frame.payload()) == ParseResult::NoError){
        if(arpm.opcode == arpm.OPCODE_REQUEST)
        { 
          if(arpm.target_ip_address == this->_ip_address.ipv4_numeric()){
            EthernetFrame reply;   ARPMessage arprep;
            reply.header().src = this->_ethernet_address;
            reply.header().dst = arpm.sender_ethernet_address;
            reply.header().type = EthernetHeader::TYPE_ARP;
            arprep.opcode = arprep.OPCODE_REPLY;
            arprep.sender_ethernet_address = this->_ethernet_address;
            arprep.sender_ip_address = this ->_ip_address.ipv4_numeric();
            arprep.target_ip_address = arpm.sender_ip_address;
            arprep.target_ethernet_address = arpm.sender_ethernet_address;
            reply.payload() = arprep.serialize();
            _frames_out.push(reply); 
          } }
        if((arpm.opcode == arpm.OPCODE_REPLY && arpm.target_ethernet_address == _ethernet_address) ||
          (arpm.opcode == arpm.OPCODE_REQUEST && arpm.target_ip_address == this->_ip_address.ipv4_numeric())){
           arp_tbl[arpm.sender_ip_address] = make_pair(arpm.sender_ethernet_address,0); 
           for(auto i = _Datagram.begin();i != _Datagram.end();)
           {
             if(i->first == arpm.sender_ip_address){
               send_datagram(i->second,_ip_address.from_ipv4_numeric(i->first));
               i = _Datagram.erase(i);
             } else i++;
           }
           timer.erase(arpm.sender_ip_address);
        } 
      }  
   }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
   for(auto i = timer.begin();i!=timer.end();)
   { 
     i->second += ms_since_last_tick;
     if(i->second >= 5000)
     { 
       ARPMessage req;
       req.opcode = req.OPCODE_REQUEST;
       req.sender_ethernet_address = this->_ethernet_address;
       req.sender_ip_address = _ip_address.ipv4_numeric();
       req.target_ethernet_address = {};
       req.target_ip_address = i->first;
       i->second = 0;
       EthernetFrame frame;
       frame.header().dst = ETHERNET_BROADCAST;
       frame.header().src = _ethernet_address;
       frame.header().type = frame.header().TYPE_ARP;
       frame.payload() = req.serialize();
       _frames_out.push(frame); 
     }else i++;
   }
   for(auto i = arp_tbl.begin();i != arp_tbl.end();)
   {   
     i->second.second += ms_since_last_tick;
     if(i->second.second >= 30000) i = arp_tbl.erase(i);
     else i++;
   }
}
