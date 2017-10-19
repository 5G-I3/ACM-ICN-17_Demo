/*
 * dashboard.js
 * Copyright (C) 2017 Martine Lenders <m.lenders@fu-berlin.de>
 *
 * Distributed under terms of the MIT license.
 */

var dashboard;

const SENSOR_SNIFFER_TOPIC = "HAW/+/gas";
const SENSOR_ALARM_ON_THRESH = -5160;
const SENSOR_ALARM_OFF_THRESH = -5160;

function getSensorName(topic) {
    return topic.split("/").slice(0, -1).join(":");
}

class Dashboard {
    constructor() {
        function onConnectionLost(responseObject) {
            if (responseObject.errorCode !== 0) {
                console.log("Dashboard:onConnectionLost:"+responseObject.errorMessage);
            }
            dashboard.connected = false;
            dashboard.tryReconnect()
        }

        function onMessageArrived(message) {
            var arrivalTime = new Date();
            var sensor = getSensorName(message.destinationName);

            console.log("Dashboard:onMessageArrived("+message.destinationName+"):"+message.payloadString);
            dashboard.updateSensor(sensor, parseInt(message.payloadString),
                                   arrivalTime);
        }

        this.sensor_list = $("#sensor-list > tbody");
        this.sensors = []
        this.connected = false;
        var host = (window.location.hostname != "") ? window.location.hostname : "localhost";
        this.client = new Paho.MQTT.Client(host, 1884, "dashboard.js");
        this.client.onConnectionLost = onConnectionLost;
        this.client.onMessageArrived = onMessageArrived;
        this.reconnectTimer = null;
        this.tryReconnect();
    }

    tryReconnect() {
        console.log("(Re)connect");
        function onConnect() {
            console.log("Dashboard:onConnect");
            dashboard.connected = true;
            if (dashboard.reconnectTimer) {
                clearTimeout(dashboard.reconnectTimer);
                dashboard.reconnectTimer = null;
            }
            dashboard.client.subscribe(SENSOR_SNIFFER_TOPIC + "/#");
        }

        if (!this.connected) {
            this.client.connect({onSuccess:onConnect});
            this.reconnectTimer = setTimeout(this.tryReconnect, 5000);
        }
    }

    updateSensor(name, value, arrivalTime) {
        var sensor_id = name.replace(/:/g, "-");
        var sensors = $.grep(this.sensors, function(s) { if (s.id == sensor_id) return s });
        var sensor, sensor_dom;

        if (sensors.length == 0) {
            sensor = {id:sensor_id, name:name, alarm:false};
            this.sensors.push(sensor);
            var $row = $('<tr id="'+sensor_id+'">' +
                             '<td>'+name+'</td>' +
                             '<td>'+value+'</td>' +
                             '<td>'+arrivalTime.toLocaleString()+'</td>' +
                         '</tr>')
            this.sensor_list.append($row);
            sensor_dom = $row;
        }
        else {
            sensor = sensors[0];
            sensor_dom = this.sensor_list.children("#" + sensor_id);
        }
        if (value != NaN) {
            if (value < SENSOR_ALARM_ON_THRESH) {
                sensor.alarm = true;
                sensor_dom.addClass("alarm")
            }
            else if (value > SENSOR_ALARM_OFF_THRESH) {
                sensor.alarm = false;
                sensor_dom.removeClass("alarm")
            }
        }
        sensor_dom.children('td:nth-child(2)').text(value);
        sensor_dom.children('td:nth-child(3)').text(arrivalTime.toLocaleString());
        console.log(arrivalTime);
        console.log(sensor);
    }
}

dashboard = new Dashboard();
