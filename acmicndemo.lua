wpan_src_f = Field.new("wpan.src64")
wpan_dst_f = Field.new("wpan.dst64")
wpan_dst_short_f = Field.new("wpan.dst16")
data_f = Field.new("data.data")
acmicndemo_proto = Proto("acmicndemo","Parse Messages from the ICN ACM Demo")
to_F = ProtoField.string("acmicndemo.to", "To")
from_F = ProtoField.string("acmicndemo.from", "From")
flow_F = ProtoField.string("acmicndemo.flow","Flow")
msg_F = ProtoField.string("acmicndemo.msg","MSG")
acmicndemo_proto.fields = {to_F,from_F,flow_F,msg_F}

MAPPING = {}
MAPPING["18:c0:ff:ee:1a:c0:ff:ee"] = "ROOT"
MAPPING["d3:c1:6d:73:ab:43:13:36"] = "NODE_A"
MAPPING["d3:c1:6d:4d:ab:0c:13:36"] = "NODE_B"
MAPPING["83:d0:6d:5e:52:a5:43:2a"] = "NODE_C"

COMPAS = 0x80
PAM = 0xC0
NAM = 0xC1
SOL = 0xC2
SNIFFER = 0x00
INTEREST = 0x05
DATA = 0x06

function acmicndemo_proto.dissector(buffer,pinfo,tree)
    local wpan_src = wpan_src_f()
    local wpan_dst = wpan_dst_f()
    local wpan_dst_short = wpan_dst_short_f()
    local data = data_f()
    if (data) then
        local datab = data.tvb:bytes()
        if (wpan_src and (wpan_dst or wpan_dst_short)) then
            local from = MAPPING[tostring(wpan_src)] or tostring(wpan_src)
            local to = MAPPING[tostring(wpan_dst)] or (wpan_dst and tostring(wpan_dst)) or (wpan_dst_short and tostring(wpan_dst_short.tvb:range(0,2)))
            local flow = from .. " -> " .. to
            local msg = "NAN"
            local subtree = tree:add(acmicndemo_proto,"ACMICNDEMO")

            local mtype = datab:get_index(0)
            if (mtype == COMPAS) then
                local sub_mtype = datab:get_index(2)
                if (sub_mtype == PAM) then
                    msg = "PAM"
                elseif (sub_mtype == NAM) then
                    msg = "NAM"
                elseif (sub_mtype == SOL) then
                    msg = "SOL"
                end
            elseif (mtype == SNIFFER) then
                msg = "SNIFFER"
            elseif (mtype == INTEREST) then
                msg = "INTEREST"
            elseif (mtype == DATA) then
                msg = "DATA"
            end

            subtree:add(from_F,from)
            subtree:add(to_F,to)
            subtree:add(flow_F,flow)
            subtree:add(msg_F,msg)
        end
    end
end

register_postdissector(acmicndemo_proto)
