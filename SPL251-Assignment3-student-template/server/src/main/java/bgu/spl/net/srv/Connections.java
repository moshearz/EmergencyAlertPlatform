package bgu.spl.net.srv;

import java.io.IOException;

public interface Connections<T> {

    void send(int connectionId, T msg);

    void send(String channel, T msg);

    void subscribe(int connectionId, String channel); //added

    void disconnect(int connectionId);
}
