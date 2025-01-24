package bgu.spl.net.srv;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class ConnectionsImpl<T> implements Connections<T> {
    private final Map<Integer, ConnectionHandler<T>> activeConnections = new ConcurrentHashMap<>();
    private final Map<String, String> inactiveConnections = new ConcurrentHashMap<>();
    private final Map<String, List<Integer>> topicSubscribers = new ConcurrentHashMap<>();

    @Override
    public void send(int connectionId, T msg) {
        ConnectionHandler<T> handler = activeConnections.get(connectionId);
        System.out.println("ConnectionsImpl: send: handler = " + handler);
        if (handler != null) {
            handler.send(msg);
        }
    }

    @Override
    public void send(String topic, T msg) {
        List<Integer> subscribers = topicSubscribers.get(topic);
        if (subscribers != null) {
            for (int connectionId : subscribers) {
                send(connectionId, msg);
            }
        }
    }

    @Override
    public void subscribe(int connectionId, String topic) { //added
        topicSubscribers.computeIfAbsent(topic, k -> new ArrayList<>()).add(connectionId);
    }

    @Override
    public void disconnect(int connectionId) {
        activeConnections.remove(connectionId);
        topicSubscribers.values().forEach(subscribers -> subscribers.remove(connectionId));
    }
}
