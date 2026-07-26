package com.smarthome.server.config;

import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.integration.annotation.ServiceActivator;
import org.springframework.integration.channel.DirectChannel;
import org.springframework.integration.mqtt.core.DefaultMqttPahoClientFactory;
import org.springframework.integration.mqtt.core.MqttPahoClientFactory;
import org.springframework.integration.mqtt.inbound.MqttPahoMessageDrivenChannelAdapter;
import org.springframework.integration.mqtt.outbound.MqttPahoMessageHandler;
import org.springframework.messaging.MessageChannel;
import org.springframework.messaging.MessageHandler;

/**
 * Two-physical-client MQTT wiring (Paho v3 / MQTT 3.1.1):
 *
 * <ul>
 *   <li><b>Inbound</b> {@code server-core}: subscribes {@code home/#} QoS 1 into a
 *       {@link DirectChannel} — Paho delivers serially on one thread, which gives free
 *       ordering (discovery before state on node boot) and removes DB races in the upsert.
 *       The adapter schedules its own reconnect attempts ({@code recoveryInterval}, default
 *       10 s) when the broker is not up yet at startup.</li>
 *   <li><b>Outbound</b> {@code server-core-pub}: async QoS 1 publisher — POST command
 *       returns 202 without waiting for delivery.</li>
 * </ul>
 *
 * Deviation from the roadmap's "single client-id server-core" (documented in README):
 * inbound adapter and outbound handler each own a physical Paho connection, and a broker
 * disconnects duplicate client IDs (reconnect kick-loop). Hence {@code server-core} +
 * {@code server-core-pub}.
 */
@Configuration
public class MqttConfig {

    @Bean
    public MqttPahoClientFactory mqttClientFactory(MqttProperties properties) {
        MqttConnectOptions options = new MqttConnectOptions();
        options.setServerURIs(new String[] {properties.uri()});
        options.setUserName(properties.username());
        options.setPassword(properties.password().toCharArray());
        options.setAutomaticReconnect(true);
        // clean session is safe: all interesting topics are retained, so every
        // (re)subscribe replays full state
        options.setCleanSession(true);

        DefaultMqttPahoClientFactory factory = new DefaultMqttPahoClientFactory();
        factory.setConnectionOptions(options);
        return factory;
    }

    @Bean
    public MessageChannel mqttInboundChannel() {
        return new DirectChannel();
    }

    @Bean
    public MessageChannel mqttOutboundChannel() {
        return new DirectChannel();
    }

    @Bean
    public MqttPahoMessageDrivenChannelAdapter mqttInbound(MqttProperties properties,
                                                           MqttPahoClientFactory factory) {
        MqttPahoMessageDrivenChannelAdapter adapter =
                new MqttPahoMessageDrivenChannelAdapter(properties.clientId(), factory, "home/#");
        adapter.setQos(1);
        adapter.setOutputChannel(mqttInboundChannel());
        return adapter;
    }

    @Bean
    @ServiceActivator(inputChannel = "mqttOutboundChannel")
    public MessageHandler mqttOutbound(MqttProperties properties, MqttPahoClientFactory factory) {
        MqttPahoMessageHandler handler =
                new MqttPahoMessageHandler(properties.clientId() + "-pub", factory);
        handler.setDefaultQos(1);
        handler.setAsync(true);
        return handler;
    }
}
