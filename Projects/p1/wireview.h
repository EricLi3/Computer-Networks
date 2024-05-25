#ifndef WIREVIEW_H
#define WIREVIEW_H

#include <map>        // for std::map
#include <string>

struct TimeStats {
    timeval start_date_time;
    double duration;
    double duration_btwn;
    std::string date_time;
    int length;
};

struct ArpPacketFields {
    unsigned short hardware_addr_format;
    unsigned short protocol_type;
    unsigned char hardware_addr_size;
    unsigned char protocol_size;
    unsigned short opcode;
};

struct PacketStats {
    std::map<int, TimeStats> time_nums;
    std::map<std::string, int> eth_senders;
    std::map<std::string, int> eth_recipients;
    std::map<std::string, int> ip_senders;
    std::map<std::string, int> ip_reciepients;
    std::map<std::string, int> source_source_arp_machines;
    std::map<std::string, int> dst_arp_machines;
    std::map<std::string, int> source_arp_ip;
    std::map<std::string, int> dst_arp_ip;
    std::map<std::string, ArpPacketFields> arp_packets_info;
    std::map<int, int> udp_src_ports;
    std::map<int, int> udp_dst_ports;
    int total_packets = 0;
    int total_packet_size = 0;
    int min_packet_size = INT_MAX;
    int max_packet_size = INT_MIN;
};

void process_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet);
void print_times(const PacketStats& packet_stats);
void unique_senders_ethernet(const PacketStats& packet_stats);
void unique_recipients_ethernet(const PacketStats& packet_stats);
void unique_senders_ip(const PacketStats& packet_stats);
void unique_recipients_ip(const PacketStats& packet_stats);
void source_arp(const PacketStats& packet_stats);
void dst_arp(const PacketStats& packet_stats);
void ip_src_arp(const PacketStats& packet_stats);
void ip_dst_arp(const PacketStats& packet_stats);
void arp_fields(const PacketStats& packet_stats);
void unique_src_ports(const PacketStats& packet_stats);
void unique_dst_ports(const PacketStats& packet_stats);
void packet_sizes(const PacketStats& packet_stats);

#endif // WIREVIEW_H
