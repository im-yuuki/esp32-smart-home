package com.smarthome.server.mqtt;

import org.springframework.integration.annotation.MessagingGateway;
import org.springframework.integration.mqtt.support.MqttHeaders;
import org.springframework.messaging.handler.annotation.Header;

/**
 * Outbound publish gateway; messages flow to the async {@code server-core-pub} Paho
 * client via {@code mqttOutboundChannel}. Boot's integration auto-configuration scans
 * {@code @MessagingGateway} interfaces in the base package — no extra
 * {@code @IntegrationComponentScan} needed.
 */
@MessagingGateway(defaultRequestChannel = "mqttOutboundChannel")
public interface MqttGateway {

    void publish(@Header(MqttHeaders.TOPIC) String topic, String payload);
}
