package bgu.spl.net.srv;


public interface Connections<T> {

    void send(int connectionId, T msg);

    void send(String channel, T msg);

    public int connect(ConnectionHandler<T> handler); //added

    void disconnect(int connectionId);

    public boolean attemptLogin(String username, String password); //added

    public void subscribe(int connectionId, String clientId, String topic); //added

    public void unsubscribe(int connectionId, String clientId); //added

    public boolean isSubscribed(int connectionId, String topic); //added
}
