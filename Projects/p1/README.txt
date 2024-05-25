In this project we parsed tcpdump data using libcap, and gathered statistics from each packet to print data such as the 
start date and time of the packet capture, the duration of the packet capture, total number of packets, Unique senders and recipients at the 
Ethernet and IP layer, machines participating in ARP, UDP ports, and Average, min, and max packet sizes. On top of these packet statistics, we were
instructed to store things in data structures to print them out at the end. 

Features:
- Analyze pcap files captured from Ethernet networks
- Extract packet statistics such as times, sizes, source and destination addresses, ARP fields, and UDP ports. 
- Calculate and display time statistics including the start time of the packet capture and duration of packet capture for each packet (respect to first packet).
- Identify and display unique senders, recipients, and ARP participents at both Ethernet and IP layers. 
- Identify and print unique source and destination UDP ports. 
- Compute and display average, mininum, and maximum packet sizes. 

Usage: 
1) In a linux environment, type 'make' into your terminal and hit 'enter'. 
2) Type './wireview INPUT_FILE' into your terminal, where INPUT_FILE is your pcap file. 

Notes: 
For the time duration for each packet capture, we calculated the difference as time since the first packet. (This matches Wireshark's output)
 