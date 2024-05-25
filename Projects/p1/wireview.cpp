#include <iostream>
#include <string.h>
#include <pcap.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include <map>        // for std::map
#include <string>

#include <net/ethernet.h> // use ether_header
#include <netinet/ip.h> // use struct iphdr
#include <netinet/if_ether.h> // use ether_arp
#include <netinet/udp.h> // use struct udphdr

#include "wireview.h" // defines struct and function prototypes
using namespace std;

int main(int argc, char *argv[]){
    if(argc != 2){
        // see if filename is provided as a CLI
        printf("Usage: %s <pacp_file>\n", argv[0]);
        return 1;
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    // open the input file
    pcap_t *handle = pcap_open_offline(argv[1], errbuf);
    if(handle == NULL){
        printf("Error in opening the file: %s\n", errbuf);
        return 1;
    }

    // check if data provided has been captured from Ethernet.
    if(pcap_datalink(handle) != DLT_EN10MB){
        printf("Not an Ethernet capture!\n");
        return 1;
    }

    struct PacketStats packet_stats;
    // proccess each packet
    if(pcap_loop(handle, -1, process_packet, (u_char*)&packet_stats) < 0){
        printf("Error in pcap Loop\n");
        return 1;
    }
    // close the pcap file
    pcap_close(handle);
    
    // Print the time stats
    print_times(packet_stats);

    // Print total number of packets
    printf("Total Number of Packets: %d\n", packet_stats.total_packets);
    printf("\n");
    printf("\n");

    // Print unique senders and recipients at Ethernet layer
    unique_senders_ethernet(packet_stats);
    unique_recipients_ethernet(packet_stats);

    // Print unique senders and recipients at IP layer
    unique_senders_ip(packet_stats);
    unique_recipients_ip(packet_stats);

    // print out source and dst ARP machines MAC and IP
    source_arp(packet_stats);
    dst_arp(packet_stats);

    ip_src_arp(packet_stats);   
    ip_dst_arp(packet_stats);
    
    // Print out the ARP fields. 
    arp_fields(packet_stats);
    
    // Print out unique source ports and destination ports seen.
    unique_src_ports(packet_stats);
    unique_dst_ports(packet_stats);
    
    // Print average, minimum, and maximum packet sizes
    packet_sizes(packet_stats);
    return 0;
}

//Define callback function to process each packet
void process_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet) {

    struct PacketStats *packet_stats = (struct PacketStats*)args; 

    // increment total packets counter
    packet_stats->total_packets++;

    int packet_number = packet_stats->total_packets;

    // length of packet in bytes
    int length = header->len;

    time_t timestamp = header->ts.tv_sec;
    struct tm *utc_time = gmtime(&timestamp);
    char time_str[26]; // Buffer to hold the formatted time string with microseconds
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", utc_time);
    
    // Append microseconds to the time string
    char micro_str[8]; // Buffer to hold the microseconds as a string (including the dot)
    snprintf(micro_str, sizeof(micro_str), ".%06ld", header->ts.tv_usec);
    strncat(time_str, micro_str, sizeof(time_str) - strlen(time_str) - 1);

    //printf("UTC Start date and time Packet %d: %s\n", packet_number, time_str);

    // Extract packet capture start date and time
    if (packet_number == 1) { // Only set start time for the first packet
    TimeStats start_stats;
    start_stats.start_date_time = header->ts;
    start_stats.duration = 0;
    start_stats.duration_btwn = 0;
    start_stats.length = length;
    start_stats.date_time = time_str;  // Store time_str in TimeStats
    packet_stats->time_nums[packet_number] = start_stats;
}   

// If packet_number > 1, update the time and duration
if (packet_number > 1) {
    long int start_sec = packet_stats->time_nums[1].start_date_time.tv_sec;
    long int start_usec = packet_stats->time_nums[1].start_date_time.tv_usec;
    long int current_sec = header->ts.tv_sec;
    long int current_usec = header->ts.tv_usec;

    long int prev_sec = packet_stats->time_nums[packet_number - 1].start_date_time.tv_sec;
    long int prev_usec = packet_stats->time_nums[packet_number - 1].start_date_time.tv_usec;

    long int duration_usec_btwn_packets =  ((current_sec - prev_sec) * 1000000) + (current_usec - prev_usec);
    double duration_sec_btwn_packets = duration_usec_btwn_packets / 1000000.0;

    long int duration_usec = ((current_sec - start_sec) * 1000000) + (current_usec - start_usec);
    double duration_sec = duration_usec / 1000000.0; // Convert microseconds to seconds

    TimeStats time;
    time.start_date_time = header->ts;
    time.duration = duration_sec;
    time.duration_btwn = duration_sec_btwn_packets;
    time.length = length;
    time.date_time = time_str;  // Store time_str in TimeStats
    packet_stats->time_nums[packet_number] = time;
}
    // Calculate packet size
    int packet_size = header->len;
    packet_stats->total_packet_size += packet_size;

    if (packet_size < packet_stats->min_packet_size) {
        packet_stats->min_packet_size = packet_size;
    }

    if (packet_size > packet_stats->max_packet_size) {
        packet_stats->max_packet_size = packet_size;
    }

    const struct ether_header* eth_header;
    const struct iphdr* ip_header;
    struct ether_arp* arp_header;
    const struct udphdr* udp_header;

    // Extract Ethernet header
    eth_header = (struct ether_header*)(packet);

    // Process Ethernet header
    char src_mac[18];
    char dst_mac[18];

    snprintf(src_mac, sizeof(src_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
             eth_header->ether_shost[0], eth_header->ether_shost[1], eth_header->ether_shost[2],
             eth_header->ether_shost[3], eth_header->ether_shost[4], eth_header->ether_shost[5]);
    snprintf(dst_mac, sizeof(dst_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
             eth_header->ether_dhost[0], eth_header->ether_dhost[1], eth_header->ether_dhost[2],
             eth_header->ether_dhost[3], eth_header->ether_dhost[4], eth_header->ether_dhost[5]);

    // Update senders and reciepients list maybe using a map. 
    packet_stats->eth_senders[src_mac]++;

    // update recipients list
    packet_stats->eth_recipients[dst_mac]++;


    // Check if packet contains IP or ARP
    if (ntohs(eth_header->ether_type) == ETHERTYPE_IP) {    // check for IP

        // Process IP packet
        ip_header = (struct iphdr *) (packet + sizeof(struct ether_header));

        // Extract IP Src and dst
        char src_ip[INET_ADDRSTRLEN];
        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ip_header->saddr), src_ip, INET_ADDRSTRLEN); // convert from binary IP to string
        inet_ntop(AF_INET, &(ip_header->daddr), dst_ip, INET_ADDRSTRLEN);

        // update the senders and reciepients lists
        packet_stats->ip_senders[src_ip]++;
        packet_stats->ip_reciepients[dst_ip]++;

        // UDP only when we have IP
        if(ip_header->protocol == IPPROTO_UDP){                      // check if a packet contains UDP
            udp_header = (struct udphdr*) (packet + sizeof(struct ether_header) + sizeof(struct iphdr));

            // convert from network order to host order
            int source_port = ntohs(udp_header -> uh_sport);
            int dst_port = ntohs(udp_header -> uh_dport);

            // Update the maps. 
            packet_stats->udp_src_ports[source_port]++;
            packet_stats->udp_dst_ports[dst_port]++;
        }
    }
    /*
    ARP    
    */
    else if (ntohs(eth_header->ether_type) == ETHERTYPE_ARP) {  // check for ARP
        // Process ARP packet
        arp_header = (struct ether_arp *)(packet + sizeof(struct ether_header));

        // Extract ARP MAC addresses if possible IP, also protocol fields
        char src_mac[18]; // accounted for the null terminating. 
        char dst_mac[18];
        
        snprintf(src_mac, sizeof(src_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
             arp_header->arp_sha[0], arp_header->arp_sha[1], arp_header->arp_sha[2],
             arp_header->arp_sha[3], arp_header->arp_sha[4], arp_header->arp_sha[5]);

        snprintf(dst_mac, sizeof(dst_mac), "%02x:%02x:%02x:%02x:%02x:%02x",
             arp_header->arp_tha[0], arp_header->arp_tha[1], arp_header->arp_tha[2],
             arp_header->arp_tha[3], arp_header->arp_tha[4], arp_header->arp_tha[5]);

        packet_stats->source_source_arp_machines[src_mac]; //store MAC addresses of machines participating in ARP
        packet_stats->dst_arp_machines[dst_mac];

        // Also get IP addrs
        char src_ip[INET_ADDRSTRLEN];
        char dst_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(arp_header->arp_spa), src_ip, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(arp_header->arp_tpa), dst_ip, INET_ADDRSTRLEN);
        packet_stats->dst_arp_ip[dst_ip]++;
        packet_stats->source_arp_ip[src_ip]++;
        
        ArpPacketFields arp_fields; // make an instance. 

        arp_fields.hardware_addr_format = ntohs(arp_header->ea_hdr.ar_hrd); // Convert to host byte order
        arp_fields.protocol_type = ntohs(arp_header->ea_hdr.ar_pro);         // Convert to host byte order
        arp_fields.hardware_addr_size = arp_header->ea_hdr.ar_hln;
        arp_fields.protocol_size = arp_header->ea_hdr.ar_pln;
        arp_fields.opcode = ntohs(arp_header->ea_hdr.ar_op);
        
        packet_stats->arp_packets_info[src_mac] = arp_fields;
    }
}


void print_times(const PacketStats& packet_stats) {
    for(const auto& [key,value] : packet_stats.time_nums){
        printf("Packet %d\n", key);
        cout <<"UTC Start date and time: "<< value.date_time << "\n";
        printf("Duration of Packet Capture From Start: %.6f seconds\n", value.duration);
        printf("Timeshift To Pervious Packet: %.9f seconds\n", value.duration_btwn);

        printf("Length of Packet in bytes: %d\n", value.length);
        printf("\n");
    }
    printf("\n");
    printf("\n");
}

void unique_senders_ethernet(const PacketStats& packet_stats){
    printf("Unique Senders at Ethernet Layer Map: |");

    for(const auto& [key,value] : packet_stats.eth_senders){
        cout << key << "|" << value << " - packets | ";
    }
    printf("\n");
    printf("\n");
}
void unique_recipients_ethernet(const PacketStats& packet_stats){
    printf("Unique Recipients at Ethernet Layer Map: |");

    // Iterate through the map and print the elements.
    for(const auto& [key,value] : packet_stats.eth_recipients){
        cout << key << "|" << value << " - packets | ";
    }
    printf("\n");
    printf("\n");
}
void unique_senders_ip(const PacketStats& packet_stats){
    printf("Unique Senders at IP Layer Map: |");
    for(const auto& [key,value] : packet_stats.ip_senders){
        cout << key << "|" << value << " - packets | ";
    }
    printf("\n");
    printf("\n");
}
void unique_recipients_ip(const PacketStats& packet_stats){
    printf("Unique Recipients at IP Layer Map: |");
    for(const auto& [key,value] : packet_stats.ip_reciepients){
        cout << key << "|" << value << " - packets | ";
    }
    printf("\n");
    printf("\n");
}
void source_arp(const PacketStats& packet_stats){
    printf("Source Machines Particiapting in ARP: |");
    for(const auto& [key,value] : packet_stats.source_source_arp_machines){
        cout << key << "|";
    }
    printf("\n");
    printf("\n");
}
void dst_arp(const PacketStats& packet_stats){
    printf("Destination Machines Participating in ARP: |");
    for(const auto& [key,value] : packet_stats.dst_arp_machines){
        cout << key << "|";
    }
    printf("\n");
    printf("\n");
}
void ip_src_arp(const PacketStats& packet_stats){
    printf("IP Addr of Source Machines Participating in ARP: |\n");
    for(const auto& [key,value] : packet_stats.source_arp_ip){
        cout << key << "|";
    }
    printf("\n");
    printf("\n");
}
void ip_dst_arp(const PacketStats& packet_stats){
    printf("IP Addr of Destination Machines Particiapting in ARP: |\n");
    for(const auto& [key,value] : packet_stats.dst_arp_ip){
        cout << key << "|";
    }
    printf("\n");
    printf("\n");
}
void arp_fields(const PacketStats& packet_stats) {
    printf("ARP fields: |");
    for (const auto& [mac_address, arp_info] : packet_stats.arp_packets_info) {
        cout << "ARP Packet with MAC Address: " << mac_address << endl;
        cout << "Hardware Address Format: " << arp_info.hardware_addr_format << endl;
        cout << "Protocol Type: " << arp_info.protocol_type << endl;
        cout << "Hardware Address Size: " << static_cast<int>(arp_info.hardware_addr_size) << endl;
        cout << "Protocol Size: " << static_cast<int>(arp_info.protocol_size) << endl;
        cout << "Opcode: " << arp_info.opcode << "\n\n" << endl;
    }
    printf("\n");
    printf("\n");
}

void unique_src_ports(const PacketStats& packet_stats) {
    printf("UDP Port Source Map: ");
    for (const auto& [port, count] : packet_stats.udp_src_ports) {
        printf("\n");
        printf("#%d - %d", port, count);
    }
    printf("\n");
    printf("\n");
}

void unique_dst_ports(const PacketStats& packet_stats) {
    printf("UDP Port Destination Map: ");
    for (const auto& [port, count] : packet_stats.udp_dst_ports) {
        printf("\n");
        printf("#%d - %d ", port, count);
    }
    printf("\n");
    printf("\n");
}

void packet_sizes(const PacketStats& packet_stats){
    float avg_packet_size = packet_stats.total_packet_size / (float)packet_stats.total_packets;
    printf("Average Packet Size: %.2f bytes\n", avg_packet_size);
    printf("Minimum Packet Size: %d bytes\n", packet_stats.min_packet_size);
    printf("Maximum Packet Size: %d bytes\n", packet_stats.max_packet_size);
}
