#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright © 2017 Martine Lenders <m.lenders@fu-berlin.de>
#
# Distributed under terms of the MIT license.

import argparse
import datetime
import json
import paho.mqtt.client as mqtt
import struct
import re
import syslog
import sys
import threading

META_DISPATCH           = 0x00

META_NODE_ID_TYPE       = 0x00
META_NODE_LABEL_TYPE    = 0x01
META_CACHE_CURRENT_TYPE = 0x02
META_CACHE_MAX_TYPE     = 0x03
META_NEW_ROUTE_TYPE     = 0x04
META_LOST_ROUTE_TYPE    = 0x05

CPS_DISPATCH            = 0x80
CPS_PAM_TYPE            = 0xc0
CPS_NAM_TYPE            = 0xc1
CPS_SOL_TYPE            = 0xc2
CPS_OPT_NAME            = 0x00

NDN_INTEREST_TYPE       = 0x05
NDN_DATA_TYPE           = 0x06
NDN_NAME_TYPE           = 0x07
NDN_NAME_COMPONENT_TYPE = 0x08

class Continue(Exception):
    pass

def get_ieee802154_data(data):
    VOID = 0x0
    SHORT = 0x2
    LONG = 0x3

    mhr_len = 3
    # read little-endian 2 byte FCF
    fcf = struct.unpack("<H", data[:2])[0]
    pkt_type = fcf & 0x0007
    dst_mode = (fcf & 0x0c00) >> 10;
    src_mode = (fcf & 0xc000) >> 14;
    pan_comp = (fcf & 0x0040)
    dst = None
    src = None

    if (pkt_type != 0x1) or (fcf & 0x0008): # ignore security enabled headers for now
        return pkt_type, None, None, 0;

    if dst_mode == SHORT:
        # skip destination PAN
        mhr_len += 2
        # read little-endian 2 byte short address
        dst = struct.unpack("<H", data[mhr_len:mhr_len+2])[0]
        if (dst == 0xffff):
            dst = "broadcast"
        else:
            dst = re.sub("(..)", r"\1:", hex(dst)[2:])[:-1]
        mhr_len += 2
    elif dst_mode == LONG:
        # skip destination PAN
        mhr_len += 2
        # read little-endian 8 byte long address
        dst = struct.unpack("<Q", data[mhr_len:mhr_len+8])[0]
        dst = re.sub("(..)", r"\1:", hex(dst)[2:])[:-1]
        mhr_len += 8
    # ignore other states => they are handled as error for now
    if not pan_comp:
        # skip source PAN
        mhr_len += 2
    if src_mode == SHORT:
        # read little-endian 2 byte short address
        src = struct.unpack("<H", data[mhr_len:mhr_len+2])[0]
        src = re.sub("(..)", r"\1:", hex(src)[2:])[:-1]
        mhr_len += 2
    elif src_mode == LONG:
        # read little-endian 8 byte long address
        src = struct.unpack("<Q", data[mhr_len:mhr_len+8])[0]
        src = re.sub("(..)", r"\1:", hex(src)[2:])[:-1]
        mhr_len += 8

    return pkt_type, dst, src, mhr_len;


def tlvs(data):
    while data:
        try:
            # read network-order struct
            # 1 byte: type
            # 1 byte: length
            type, length = struct.unpack('!BB', data[:2])
            # read `length` bytes as value
            value = struct.unpack('!%is'%length, data[2:2+length])[0]
        except:
            syslog.syslog("Unproper TLV structure found: %s" % data)
            break
        yield type, value
        data = data[2+length:]

def compas_tlvs(data):
    while data:
        try:
            # read little-endian struct
            # 1 byte: type
            # 2 byte: length
            type, length = struct.unpack('<BH', data[:3])
            value = struct.unpack('!%is'%length, data[3:3+length])[0]
        except:
            syslog.syslog("Unproper TLV structure found: %s" % data)
            break
        yield type, value
        data = data[3+length:]

def get_varnum(data):
    # read network-order 1 byte
    num = struct.unpack('!B', data[0:1])[0]
    if (num < 253):
        return num,1
    elif (num == 253):
        # read network-order 2 byte
        num = struct.unpack('!H', data[1:3])[0]
        return num,3
    elif (num == 254):
        # read network-order 4 byte
        num = struct.unpack('!L', data[1:5])[0]
        return num,5
    else:
        # read network-order 8 byte
        num = struct.unpack('!Q', data[1:9])[0]
        return num,9

def parse_name(data):
    type, length, component = parse_ndn(data)
    data = data[length:]
    name = b"/" + component
    while type == NDN_NAME_COMPONENT_TYPE and (len(data) > 0):
        type, length, component = parse_ndn(data)
        data = data[length:]
        name += (b"/" + component)
    return name

def parse_ndn(data):
    try:
        type, ot = get_varnum(data)
        length, ol = get_varnum(data[ot:])
        data = data[ot+ol:]
        if (type == NDN_DATA_TYPE) or (type == NDN_INTEREST_TYPE):
            _, length, name = parse_ndn(data)
            return type, length, name
        elif (type == NDN_NAME_TYPE):
            name = parse_name(data[:length])
            return type, len(name), name
        elif (type == NDN_NAME_COMPONENT_TYPE):
            # read `length` byte name component
            value = struct.unpack('!%is'%length, data[:length])[0]
            return type, length+ot+ol, value
        else:
            raise ValueError("Unexpected type %u" % type)
    except:
        raise ValueError("Unexpected NDN packet %s" % data)

def get_ndn_type_and_name(data):
    type, _, name = parse_ndn(data)
    return type, name

class SnifferClient(object):
    def __init__(self, mqtt_host, mqtt_port):
        self.client = mqtt.Client(userdata=self)
        self.client.connect(mqtt_host, mqtt_port)
        self.client.loop_start()
        self.pkt_count = 0;

    def __del__(self):
        self.client.loop_stop()

    def publish_pkt(self, data, time):
        type, dst, src, mhr_len = get_ieee802154_data(data)

        if (type != 0x1) or (dst == None) or (src == None):
            # only parse data packets
            return;
        data = data[mhr_len:]
        topic = ""
        payload = ""

        if int(data[0]) == META_DISPATCH:
            topic = "sniffer/network/%s" % src
            meta_list = []
            cache_info = None
            for type, value in tlvs(data[1:]):
                if type == META_NODE_ID_TYPE:
                    value = value.decode(errors='ignore')
                    value = value if value != "" else src
                    value = re.sub("^[^A-Za-z0-9]*", "", value)
                    value = re.sub("[^_A-Za-z0-9-.]", "", value)
                    meta_list.append({"type": "node",
                                      "value": {"addr": src,
                                                "id": value}})
                elif type == META_NODE_LABEL_TYPE:
                    pass # ignore for now
                elif type == META_CACHE_CURRENT_TYPE:
                    if cache_info:
                        cache_info["value"]["cached"] = int(value[0])
                    else:
                        cache_info = {"type": "cache-info",
                                      "value": {"addr": src,
                                                "cached": int(value[0])}}
                        meta_list.append(cache_info)
                elif type == META_CACHE_MAX_TYPE:
                    if cache_info:
                        cache_info["value"]["cache_size"] = int(value[0])
                    else:
                        cache_info = {"type": "cache-info",
                                      "value": {"addr": src,
                                                "cache_size": int(value[0])}}
                        meta_list.append(cache_info)
                elif type == META_NEW_ROUTE_TYPE:
                    meta_list.append({"type": "route",
                                      "value": {"src": src,
                                                "dst": ":".join(('%02x' % x) for x in value)}})
                elif type == META_LOST_ROUTE_TYPE:
                    meta_list.append({"type": "route-list",
                                      "value": {"src": src,
                                                "dst": ":".join(('%02x' % x) for x in value)}})
            payload = json.dumps(meta_list)
        else:
            topic = "sniffer/pkt/%i" % self.pkt_count
            self.pkt_count += 1
            pkt = { "dst": dst, "src": src, "time": time.isoformat() }
            if data[0] == CPS_DISPATCH:
                data = data[2:]
                if int(data[0]) == CPS_PAM_TYPE:
                    pkt["type"] = "pam"
                    # read little-endian struct
                    # 1 byte: type
                    # 4 byte: padding
                    # 2 byte: prefix length
                    type, pfx_len = struct.unpack("<B4xH", data[:7])
                    # read network-order `length` byte prefix
                    if (pfx_len != len(data[7:pfx_len+7])):
                        # XXX evil hack, let's fix that in the future
                        return
                    pfx = struct.unpack("!%is"%pfx_len, data[7:pfx_len+7])[0]
                    pkt["label"] = pfx.decode(errors='ignore')
                elif int(data[0]) == CPS_NAM_TYPE:
                    pkt["type"] = "nam"
                    for type, value in compas_tlvs(data[2:]):
                        if (type == CPS_OPT_NAME):
                            pkt["label"] = value.decode(errors='ignore')
                elif int(data[0]) == CPS_SOL_TYPE:
                    pkt["type"] = "sol"
                else:
                    pkt["type"] = "unknown"
            else:
                try:
                    type, name = get_ndn_type_and_name(data)
                except ValueError as e:
                    syslog.syslog(str(e))
                    return
                if type == NDN_DATA_TYPE:
                    pkt["type"] = "data"
                    pkt["label"] = name.decode(errors='ignore')
                elif type == NDN_INTEREST_TYPE:
                    pkt["type"] = "interest"
                    pkt["label"] = name.decode(errors='ignore')
                else:
                    pkt["type"] = "unknown"
            payload = json.dumps(pkt)
        self.client.publish(topic, payload)

def read_pcap_hdr(pcap_file):
    data = pcap_file.read(24)

    if len(data) != 24:
        syslog.syslog("Unable to read PCAP header")
        sys.exit(1)

    # read host-order struct
    #  4 byte: magic number (to get PCAP byte-order)
    # 16 byte: padding
    #  4 byte: network (i.e. frame type) identifier
    magic_number, network = struct.unpack("=L16xL", data)

    if (magic_number != 0xa1b2c3d4):
        # only support host's byte order for now (we only read locally generated
        # PCAPs anyway)
        syslog.syslog("PCAP was not in native byteorder")
        sys.exit(1)
    if (network == 113):
        # PCAP is Linux cooked frame, we need to do further (non-802.15.4) parsing
        return True, True
    if (network not in [195,230]):
        syslog.syslog("PCAP does not contain pure IEEE 802.15.4 MAC (network %u)" % network)
        sys.exit(1)
    # return that FCS is provided and no further (non-802.15.4) parsing is required
    return network != 230, False

def read_pcap_rec_hdr(data, read_linux_cooked=False):
    # read host-order struct
    # 4 byte: seconds timestamp (in UNIX time)
    # 4 byte: microseconds timestamp (in UNIX time)
    # 4 byte: included length of frame
    # 4 byte: original length of frame
    ts_sec, ts_usec, incl_len, orig_len = struct.unpack("=LLLL", data[:16])
    if read_linux_cooked:   # network type is of linux cooked frames, we need to parse that
        # read network-order struct
        # 14 byte: padding
        #  2 byte: network (i.e. frame type) identifier
        network = struct.unpack("!14xH",  data[16:32])[0]
        if network != 0x00f6:
            syslog.syslog("PCAP does not contain pure IEEE 802.15.4 MAC (Linux cooked %04x)" % network)
            raise Continue()
        incl_len -= 18 # linux cooked header frame + FCS

    time = datetime.datetime.fromtimestamp(ts_sec + (ts_usec / 1000000))
    return time, incl_len

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input-pcap",
                        help="PCAP file ('-' for stdin)", type=str,
                        default='-')
    parser.add_argument("-H", "--host",
                        help="MQTT broker host", type=str)
    parser.add_argument("-p", "--port",
                        help="MQTT broker port (default: 1883)", type=int,
                        default=1883)
    args = parser.parse_args()
    pcap_file = sys.stdin

    if args.input_pcap != "-":
        pcap_file = open(args.input_pcap, "rb")

    client = SnifferClient(args.host, args.port)
    has_fcs, read_linux_cooked = read_pcap_hdr(pcap_file)

    while True:
        try:
            if read_linux_cooked:
                rec_hdr = pcap_file.read(32)
            else:
                rec_hdr = pcap_file.read(16)
            if len(rec_hdr) != 16 and not (read_linux_cooked and len(rec_hdr) == 32):
                break
            time, length = read_pcap_rec_hdr(rec_hdr, read_linux_cooked)
            data = pcap_file.read(length)
            if len(data) != length:
                break
            if has_fcs:
                pcap_file.read(2)   # read FCS, but ignore
            client.publish_pkt(data, time)
        except Continue:
            continue

    if (args.input_pcap != "-"):
        pcap_file.close()
