# TCP/IP STACK C

## GOALS
- Implement Data Encapsulation amd De-encapsulation.
- Implement Shortest Path Algorithm(Dijkstra Algorithm).
- Packet exchange simulation.
- ARP
- L2 MAC based Forwarding.
- Ping
- L3 routing
- Dynamic construction of L3 routing
- Implemented practical version of OSI model.
- Implemented all the logic to parse the packet content, evaluate packet hdr content and take decision what to do with the packet.
- Implemented VLAN based forwarding with Access ports and trunk ports
- Cooked up the packets from the scratch
- Implemented ARP and ARP table.
- Implemented L2 switching.
- On demand ARP resolution.

## TO DO NEXT
- [x] To implement PING application. 
- [ ] To implement Automatic Routing Table construction.
- [ ] To implement ECMP, Loop Free and Shortest Path Route.
- [ ] To implement LOGS
- [ ] To implement NPM(neighbour management protocol).
- [ ] To implement Timer.
- [ ] To implement Event notification.

## BUGS TO FIX
- [x] Interface UP DOWN.
- [ ] IP conversion. 

Made a practical version of the OSI model. Implemented Data Link Layer(L2) and Network Layer(L3) from scratch. Also implemented L2 MAC and VLAN based forwarding.
Built a Multinode Topology Emulation of Routers and Switches. Implemented L2, including ARP and L3 routing
Created a Multinode Topology Emulation with Routers and Switches. Engineered Data Link Layer (L2) and Network Layer (L3) from scratch, incorporating L2 MAC and VLAN-based forwarding for efficient data transmission.
Crafted packets from scratch, implementing the entire logic for parsing packet content, evaluating header details, and making informed decisions about packet handling. Additionally, integrated various protocols, including ARP, for comprehensive network functionality.


Developed a C++ socket programming library over POSIX, enabling efficient management of multiple clients through multiplexing and concurrency. The library notifies the application about new client connections, disconnections, and received messages. Additionally, it ensures a graceful shutdown of the server for enhanced reliability.
