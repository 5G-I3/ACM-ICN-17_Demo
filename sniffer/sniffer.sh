#! /bin/bash
#
# sniffer.sh
# Copyright (C) 2017 Martine Lenders <mail@martine-lenders.eu>
#
# Distributed under terms of the MIT license.
#

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

mv /home/pi/sniff.1.pcap /home/pi/sniff.2.pcap 2> /dev/null
mv /home/pi/sniff.pcap /home/pi/sniff.1.pcap 2> /dev/null
"${DIR}"/pcap_parser.py -i <(dumpcap -i monitor0 -P -w - -s0 | tee -a /home/pi/sniff.pcap) -H i3-gateway -p 1883
