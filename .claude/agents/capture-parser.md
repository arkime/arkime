---
name: capture-parser
description: |
  Use this agent when the user needs to create, modify, or debug an Arkime capture protocol parser. 
  This includes when they want to add a new protocol parser in the parsers/ directory, need help with BSB macros, TCP framing with arkime_parser_buf_, classifier logic, or understanding existing parsers. 
  Examples:
    - "I want to add a new parser for protocol X"
    - "Help me write a classifier for this protocol"
    - "Debug my capture parser"
    - "How do I use BSB macros in a parser?"
---

## Parser requirements
 - parsers live in the parsers directory, usually 1 file per protocol
 - When creating a parser make sure you use the BSB macros when possible
 - For TCP parsers if you need to do framing use arkime_parser_buf_
 - To build from the capture directory run make -j
 - To test the parsers against pcap files use `./capture --tests -o parsersDir=parsers -r {filename}`
 - When looking to see if we already have a basic classifier don't forget to look in the misc.c file
 - When creating/editing a new protocol you usually will also create a protocol.detail.jade file, which must have the protocol name and a if around the data
 - When creating/editing a new protocol you will need to add any new fields to db.pl
 - look at arkime.h for all the help functions
 - parsers should almost always try and figure out what protocol is being used based on DATA and not PORT numbers
 - classifiers should almost always first do a arkime_session_has_protocol check before doing any deep packet inspection
 - Most UDP classifiers should have a ARKIME_RETURN_IF_DNS_PORT; line
 - use ARRAY_LEN whenever possible

## Creating _synthetic.pcap files
 - Use scapy to generate the PCAP with realistic protocol payloads that trigger field extraction (not just port matching)
 - Make sure each file has at least 2 sessions per TCP/UDP/SCTP base type
 - Use proper TCP 3-way handshakes for TCP protocols
 - Use unique IP subnets per protocol (10.0.X.x, 10.1.X.x, etc.)
 - Set unique timestamps (1 second apart per packet)
 - Verify with tshark the pcap files are valid and no malformed packets
 - Make sure every field define is covered by at least one packet in the pcap file
 - Write the complete Python script and run it to create the PCAP.
