/*
 * sniffer.js
 * Copyright (C) 2017 Martine Lenders <m.lenders@fu-berlin.de>
 *
 * Distributed under terms of the MIT license.
 */

var graph;
const DEFAULT_GRAPH_WIDTH = $("#network").width();
const DEFAULT_GRAPH_HEIGHT = $("#network").height();
const DEFAULT_CHARGE_STRENGTH = -3000;
const DEFAULT_LINK_DIST = 200;
const UC_ANIMATION_STEPS = 10;
const UC_ANIMATION_INTERVAL = 10;
const NODE_HEIGHT = 25;
const NODE_WIDTH = 180;
const SNIFFER_TOPIC = "sniffer"
const NETWORK_ARCH_TOPIC = "sniffer/network"
const PACKET_TOPIC = "sniffer/pkt"

class Graph {
    constructor() {
        this.vis = d3.select("#network")
        this.w = this.vis.attr("width")
        this.h = this.vis.attr("height")
        this.nodes = [];
        this.links = [];
        this.selectElements();
        this.simulation = d3.forceSimulation(this.nodes)
            .force("center", d3.forceCenter(this.w / 2, this.h / 2))
            .force("charge", d3.forceManyBody().strength(DEFAULT_CHARGE_STRENGTH))
            .force("link", d3.forceLink(this.links).distance(DEFAULT_LINK_DIST))
            .force("x", d3.forceX())
            .force("y", d3.forceY())
            .alphaTarget(0.1)
            .on("tick", this.ticked);

        this.update();
    }

    selectElements() {
        this.link = this.vis.selectAll("line")
            .data(this.links, function(d) {
                return d.source.id + "-" + d.target.id;
            });
        this.node = this.vis.selectAll("g.node")
            .data(this.nodes, function(d) {
                return d.id;
            });
    }

    addNode(node) {
        if (this.findNode(node.id)) {
            return;
        }
        this.nodes.push(node);
        this.update();
    }

    removeNode(id) {
        var i = 0;
        var node = this.findNode(id);

        while (i < this.links.length) {
            if ((this.links[i]['source'] == node) ||
                (this.links[i]['target'] == node)) {
                this.links.splice(i, 1);
            }
            else {
                i++;
            }
        }
        this.nodes.splice(this.findNodeIndex(id), 1);
        this.update();
    }

    removeAllNodes() {
        this.nodes.splice(0, this.links.length);
        this.update();
    };

    findNode(id) {
        for (var i in this.nodes) {
            if (this.nodes[i]["id"] === id) {
                return this.nodes[i];
            }
        }
    }

    findNodeByAddress(addr) {
        for (var i in this.nodes) {
            if (this.nodes[i]["addr"] === addr) {
                return this.nodes[i];
            }
        }
    }

    findNodeIndex(id) {
        for (var i = 0; i < this.nodes.length; i++) {
            if (this.nodes[i].id == id) {
                return i;
            }
        }
    }

    updateNodeCache(node, rel)
    {
        if (typeof node === "undefined") {
            return;
        }

        var cache = d3.select("g#" + node.id + "-cache");

        if (!cache.empty()) {
            var max_height = cache.select(".cache-meter-frame").attr("height") - 4;
            var height = ((1 - rel) * max_height);

            if (rel > 1) {
                rel = 1;
            }
            else if (rel < 0) {
                rel = 0;
            }
            cache.select(".cache-meter")
                .attr("y", height)
                .attr("height", max_height - height);

        }
    }

    addLink(source, target, c="") {
        if (this.findLink(source, target)) {
            return;
        }
        this.links.push({"source": this.findNode(source),
                         "target": this.findNode(target),
                         "class": c})
        this.update()
    }

    removeLink(source, target) {
        if (!this.findLink(source, target)) {
            // prevent jerking around
            return;
        }
        for (var i = 0; i < this.links.length; i++) {
            if ((this.links[i].source.id == source) &&
                (this.links[i].target.id == target)) {
                this.links.splice(i, 1);
                break;
            }
        }
        this.update();
    }

    findLink(source, target) {
        for (var i = 0; i < this.links.length; i++) {
            if ((this.links[i].source.id == source) &&
                (this.links[i].target.id == target)) {
                return this.links[i];
            }
        }
    }

    removeAllLinks() {
        this.links.splice(0, this.links.length);
        this.update();
    };

    addBroadcast(source, type="") {
        var n = d3.select("g#" + source);

        n.append("circle")
            .attr("cx", 0)
            .attr("cy", 0)
            .attr("r", Math.min(NODE_HEIGHT, NODE_WIDTH) / 2)
            .attr("class", "bc " + type)
            .attr("id", "bc-" + source)
            .lower();
        d3.timeout(function() {
            n = d3.select("#bc-" + source).remove();
        }, 1000)
    }

    addUnicast(source, target, type="") {
        var src = this.findNode(source)
        var tgt = this.findNode(target)
        var step = 0;

        if ((typeof src === "undefined") &&
            (typeof tgt === "undefined")) {
            return;
        }

        this.vis.insert("circle", "g.node") // fails if there are no nodes
            .attr("cx", src.x)
            .attr("cy", src.y)
            .attr("r", 10)
            .attr("class", "uc " + type)
            .attr("id", "uc-" + source + "-" + target);
            var t = d3.timer(function() {
                var n = d3.select("#uc-" + source + "-" + target);

                if (step < UC_ANIMATION_STEPS) {
                    step++;
                    n.attr("cx", src.x + (step / UC_ANIMATION_STEPS) * (tgt.x - src.x));
                    n.attr("cy", src.y + (step / UC_ANIMATION_STEPS) * (tgt.y - src.y));
                }
                else {
                    n.remove();
                    t.stop();
                }
            }, UC_ANIMATION_INTERVAL);
    }

    ticked() {
        graph.node.attr("transform", function (d) {
            var transform = "translate(" + d.x + "," + d.y + ")";

            if (d.gateway) {
                transform += " scale(1.2)"
            }
            return transform
        });

        graph.link.attr("x1", function (d) {
                return d.source.x;
            })
            .attr("y1", function (d) {
                return d.source.y;
            })
            .attr("x2", function (d)  {
                return d.target.x;
            })
            .attr("y2", function (d)  {
                return d.target.y;
            });
    }

    update() {
        this.selectElements();
        var nodeEnter;

        /* determine node degrees */
        this.nodes.forEach(function(d) {
            d.degree = 0;
        });
        this.links.forEach(function(d) {
            graph.nodes[graph.findNodeIndex(d.source.id)].degree++;
            graph.nodes[graph.findNodeIndex(d.target.id)].degree++;
        });
        /* link DOM */
        this.link.enter().append("line")
            .lower()
            .attr("id", function(d) {
                return d.source.id + "-" + d.target.id;
            })
            .attr("class", function(d) {
                if (d.class != "") {
                    return "link " + d.class;
                }
                else {
                    return "link";
                }
            });
        this.link.exit().remove();
        /* node DOM */
        nodeEnter = this.node.enter().append("g")
            .attr("id", function(d) {
                return d.id;
            });
        nodeEnter.append("rect")
            .attr("x", - NODE_WIDTH / 2)
            .attr("y", - NODE_HEIGHT / 2)
            .attr("width", NODE_WIDTH)
            .attr("height", NODE_HEIGHT)
            .attr("id", function(d) {
                return d.id;
            })
            .attr("class", "nodeFrame");
        nodeEnter.append("text")
            .attr("x", (- NODE_WIDTH / 2) + 2)
            .attr("y", (- NODE_HEIGHT / 2) + 10)
            .attr("class", "nodeID")
            .text(function(d) {
                if (d.gateway && (d.id.toLowerCase() != "gateway")) {
                    return d.id + " (gateway)"
                }
                return d.id;
            });
        nodeEnter.append("text")
            .attr("x", (- NODE_WIDTH / 2) + 7)
            .attr("y", (- NODE_HEIGHT / 2) + 21)
            .attr("class", "nodeInfo")
            .text(function(d) {
                return "Address: " + d.addr;
            });
        var cache_meter = nodeEnter.append("g")
            .attr("id", function(d) {
                return d.id + "-cache";
            })
            .attr("transform",
                  "translate(" + ((NODE_WIDTH / 2) - 11) + "," +
                                 (- (NODE_HEIGHT / 2) + 5) + ")");
        cache_meter.append("rect")
                .attr("x", -2)
                .attr("y", -2)
                .attr("width", 10)
                .attr("height", NODE_HEIGHT - 6)
                .attr("class", "cache-meter-frame")
        cache_meter.append("rect")
                .attr("x", 0)
                .attr("y", NODE_HEIGHT - 4)
                .attr("width", 6)
                .attr("height", 0)
                .attr("class", "cache-meter")
        this.node.exit().remove();

        this.nodes.forEach(function(d) {
            d3.select("#" + d.id)
                .attr("class", function (d) {
                    if ((d.degree > 0) || d.gateway) {
                        if (d.gateway) {
                            return "node gateway"
                        }
                        return "node";
                    }
                    else {
                        return "node unreachable";
                    }
                })
        });

        this.selectElements();
        // Update and restart the simulation.
        this.simulation.nodes(this.nodes);
        this.simulation.force("link").links(this.links);
        this.simulation.alpha(1).restart();
    }

    updateSize() {
        this.w = $("#network").width()
        this.h = $("#network").height()
        var charge = (Math.min(this.w, this.h) /
                      Math.min(DEFAULT_GRAPH_WIDTH, DEFAULT_GRAPH_HEIGHT)) *
                     DEFAULT_CHARGE_STRENGTH;
        var distance = (Math.min(this.w, this.h) /
                        Math.min(DEFAULT_GRAPH_WIDTH, DEFAULT_GRAPH_HEIGHT)) *
                       DEFAULT_LINK_DIST;
        this.simulation.force("center", d3.forceCenter(this.w / 2, this.h / 2))
            .force("charge", d3.forceManyBody().strength(charge))
            .force("link", d3.forceLink(this.links).distance(distance))

        this.update();
    }
}

graph = new Graph();
graph.updateSize();

$(window).resize(function () {
    graph.updateSize();
})

var BSCOLOR_MAPPING = {
    "data": "primary",
    "interest": "warning",
    "pam": "success",
    "nam": "danger",
    "sol": "info",
}

function get_bscolor(type) {
    if (type in BSCOLOR_MAPPING) {
        return BSCOLOR_MAPPING[type];
    }
    else {
        return "secondary";
    }
}

function add_packet(pkt)
{
	if (pkt.type == "unknown") {
		return;
	}
    var pkt_list = $("#pkt-list");
    var dst_str = pkt.dst, src_str = pkt.src;
    var dst = graph.findNodeByAddress(pkt.dst);
    var src = graph.findNodeByAddress(pkt.src);

    if (typeof dst !== "undefined") {
        dst_str += " (" + dst.id + ")"
    }
    if (typeof src !== "undefined") {
        src_str += " (" + src.id + ")"
    }

    pkt_list.prepend('<li class="list-group-item flex-column align-items-start list-group-item-' +
                     get_bscolor(pkt.type) + '">' +
                     '<div class="d-flex w-100 justify-content-between">' +
                       '<h5 class="mb-1">' + pkt.type.toUpperCase() +
                       ((pkt.label) ? ' <small class="text-muted">' + pkt.label + '</small>' : '') +
                      '</h5>' +
                       '<small>' + pkt.time + '</small>' +
                     '</div>' +
                     '<p class="mb-1">' +
                       '<strong>Destination:</strong> ' + dst_str + '<br />' +
                       '<strong>Source</strong> ' + src_str +
                     '</p>' +
                     '</li>');
    /* remove cycled out elements */
    while (pkt_list.height() > $(window).height()) {
        pkt_list.children().last().remove();
    }

    if (typeof src !== "undefined") {
        if (pkt.dst.toLowerCase() == "broadcast") {
            graph.addBroadcast(src.id, pkt.type);
        }
        else if (typeof dst !== "undefined") {
            graph.addUnicast(src.id, dst.id, pkt.type);
        }
    }
}

var mqttReconnectTimer = null;
var mqttConnected = false;
var host = (window.location.hostname != "") ? window.location.hostname : "localhost"
client = new Paho.MQTT.Client(host, 1884, "sniffer.js");

// set callback handlers
client.onConnectionLost = onConnectionLost;
client.onMessageArrived = onMessageArrived;

// connect the client
client.connect({onSuccess:onConnect});


// called when the client connects
function onConnect() {
    // Once a connection has been made, make a subscription and send a message.
    console.log("onConnect");
    mqttConnected = true;
    if (mqttReconnectTimer) {
        clearTimeout(mqttReconnectTimer);
        mqttReconnectTimer = null;
    }
    client.subscribe(SNIFFER_TOPIC + "/#");
}

// called when the client loses its connection
function onConnectionLost(responseObject) {
    if (responseObject.errorCode !== 0) {
        console.log("onConnectionLost:"+responseObject.errorMessage);
    }
    mqttConnected = false;
    tryReconnect();
}

function tryReconnect() {
    console.log("Re(connect)");
    if (!mqttConnected) {
        client.connect({onSuccess:onConnect});
        mqttReconnectTimer = setTimeout(tryReconnect, 5000)
    }
}

// called when a message arrives
function onMessageArrived(message) {
    console.log("onMessageArrived("+message.destinationName+"):"+message.payloadString);
    arrival = new Date()
    if (!message.destinationName.startsWith(SNIFFER_TOPIC)) {
        return;
    }
    if (!message.payloadString) {
        return;
    }

    json = JSON.parse(message.payloadString);

    if (message.destinationName.startsWith(PACKET_TOPIC)) {
        add_packet(json);
    }
    if (message.destinationName.startsWith(NETWORK_ARCH_TOPIC)) {
        json.forEach(function(info) {
            if (info.type == "node") {
                graph.addNode(info.value);
            }
            else if (info.type == "cache-info") {
                if (!("cache_size" in info.value) ||
                    !("cached" in info.value)) {
                    // only accept full information
                    return;
                }
		var node = graph.findNodeByAddress(info.value.addr)
                graph.updateNodeCache(node, info.value.cached / info.value.cache_size);
            }
            else if (info.type == "route") {
                var dst = graph.findNodeByAddress(info.value.dst);
                var src = graph.findNodeByAddress(info.value.src);

                if ((typeof dst === "undefined") ||
                    (typeof src === "undefined")) {
                    return;
                }
                graph.addLink(src.id, dst.id);
            }
            else if (info.type == "route-lost") {
                var dst = graph.findNodeByAddress(info.value.dst);
                var src = graph.findNodeByAddress(info.value.src);

                if ((typeof dst === "undefined") ||
                    (typeof src === "undefined")) {
                    return;
                }
                graph.removeLink(src.id, dst.id);
            }
        });
    }
}
