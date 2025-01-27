package bgu.spl.net.srv;

import java.io.IOException;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

import bgu.spl.net.impl.stomp.StompFrame;

public class ConnectionsImpl implements Connections<StompFrame> {
    private final Map<Integer, ConnectionHandler<StompFrame>> activeConnections = new ConcurrentHashMap<>();
    private final Map<String, Map<Integer, String>> topicSubscribers = new ConcurrentHashMap<>(); //change set to map of server given id and client given id
    private final ConcurrentHashMap<String, String> registedClients = new ConcurrentHashMap<>();
    private final AtomicInteger idCounter = new AtomicInteger(1);

    @Override
    public void send(int connectionId, StompFrame msg) {
        ConnectionHandler<StompFrame> handler = activeConnections.get(connectionId);
        System.out.println("ConnectionsImpl: send: handler = " + handler);
        if (handler != null) {
            handler.send(msg);
        }
    }

    @Override
    public void send(String topic, StompFrame msg) {
        if (msg.getHeader("destination").equals(topic)) {
            Map<Integer, String> subscribers = topicSubscribers.get(topic);
            subscribers.forEach((connectionId, clientId) -> {
                msg.getHeaders().put("subscription", clientId);
                send(connectionId, msg);
            });
        }
    }

    @Override
    public int connect(ConnectionHandler<StompFrame> handler) {
        int connectionId = idCounter.getAndIncrement();
        activeConnections.put(connectionId, handler);
        return connectionId;
    }

    @Override
    public void disconnect(int connectionId) {
        try {
            topicSubscribers.forEach((topic, subscribers) -> {
                subscribers.remove(connectionId);
            });
            activeConnections.remove(connectionId).close();
        } catch (IOException ignore) {}
    }

    @Override
    public boolean attemptLogin(String username, String password) {
        if (!registedClients.containsKey(username)) {
            registedClients.put(username, password);
        }
        return registedClients.get(username).matches(password);
    }

    @Override
    public void subscribe(int connectionId, String clientId, String topic) {
        if (activeConnections.containsKey(connectionId)) {
            topicSubscribers.computeIfAbsent(topic, key -> new ConcurrentHashMap<>());
            topicSubscribers.get(topic).put(connectionId, clientId);
        }
    }

    @Override
    public void unsubscribe(int connectionId, String clientId) {
        for (Map<Integer, String> subscribers : topicSubscribers.values()) {
            String registedClientId = subscribers.get(connectionId);
            if (registedClientId.equals(clientId)) {
                System.out.println("removing connectionId: " + connectionId);
                subscribers.remove(connectionId);
            }
        }
    }

    @Override
    public boolean isSubscribed(int connectionId, String topic) {
        if (topicSubscribers.containsKey(topic) && topicSubscribers.get(topic).containsKey(connectionId)) {
            return true;
        }
        return false;
    }
}
