package bgu.spl.net.impl.stomp;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;

public class StompMessagingProtocolImpl  implements StompMessagingProtocol<StompFrame> {
    private int connectionId;
    private Connections<StompFrame> connections;
    private boolean shouldTerminate = false;
    private final AtomicInteger messageIdCounter = new AtomicInteger(1);
    private String username = "";


    @Override
    public void start(int connectionId, Connections<StompFrame> connections) {
        System.out.println("in StompMessagingProtocolImpl: start");
        this.connectionId = connectionId;
        this.connections = connections;
    }

    @Override
    public void process(StompFrame frame) {
        System.out.println("in StompMessagingProtocolImpl: process");
        String command = frame.getCommand();
        switch (command) {
            case "CONNECT":
                connect(frame);
                break;
            case "SUBSCRIBE":
                subscribe(frame);
                break;
            case "UNSUBSCRIBE":
                unsubscribe(frame);
                break;
            case "SEND":
                send(frame);
                break;
            case "DISCONNECT":
                disconnect(frame);
                break;
            default:
                sendErrorFrame(new ConcurrentHashMap<>(), "Unknown command", "The command '" + command + "' isn't legal.");
                break;
        }
    }

    @Override
    public boolean shouldTerminate() {
        //System.out.println("in StompMessagingProtocolImpl: shouldTerminate");
        return shouldTerminate;
    }

    private void connect(StompFrame frame) {
        System.out.println("in StompMessagingProtocolImpl: connect");
        if (frame.getHeader("accept-version") == null) {
            sendErrorFrame(frame.getHeaders(), "Missing 'accept-version' header", "CONNECT frame must include an 'accept-version' header");
            connections.disconnect(connectionId);
            return;
        }
        if (frame.getHeader("host") == null) {
            sendErrorFrame(frame.getHeaders(), "Missing 'host' header", "CONNECT frame must include a 'host' header");
            connections.disconnect(connectionId);
            return;
        }
        if (!connections.attemptLogin(frame.getHeader("login"), frame.getHeader("passcode"))) {
            sendErrorFrame(frame.getHeaders(), "Wrong passcode", "Said username is already registed in server data base under a different passcode.");
            connections.disconnect(connectionId);
            return;
        }
        if (connections.isOnline(frame.getHeader("login"))) {
            sendErrorFrame(frame.getHeaders(), "User already online", "Said username is already online.");
            connections.disconnect(connectionId);
            return;
        }
        username = frame.getHeader("login");
        connections.login(frame.getHeader("login"), frame.getHeader("passcode"), connectionId);
        Map<String, String> headers = new ConcurrentHashMap<>();
        headers.put("version", "1.2");
        StompFrame response = new StompFrame(
            "CONNECTED", 
            headers, 
            null
        );
        connections.send(connectionId, response);
    }

    private void subscribe(StompFrame frame) {
        System.out.println("in StompMessagingProtocolImpl: subscribe");
        String topic = frame.getHeader("destination");
        String id = frame.getHeader("id");

        if (topic == null) {
            System.out.println("missing topic");
            sendErrorFrame(frame.getHeaders(), "Missing 'destination' header", "SUBSCRIBE frame must include a 'destination' header.");
            return;
        }
        if (id == null) {
            System.out.println("missing id");
            sendErrorFrame(frame.getHeaders(), "Missing 'id' header", "SUBSCRIBE frame must include an 'id' header.");
            return;
        }
        
        if (!connections.isSubscribed(connectionId, topic)) {
            connections.subscribe(connectionId, id, topic);
            String receipt = frame.getHeader("receipt");
            if (receipt != null) {
                sendReceiptFrame(receipt);
            }   
        } else {
            System.out.println("already subscribed");
            sendErrorFrame(frame.getHeaders(), "Already subscribed", "The user is already subscribed to said 'topic'.");
        }
    }

    private void unsubscribe(StompFrame frame) {
        System.out.println("in StompMessagingProtocolImpl: unsubscribe");
        String id = frame.getHeader("id");

        if (id == null) {
            sendErrorFrame(frame.getHeaders(), "Missing 'id' header", "UNSUBSCRIBE frame must include an 'id' header");
            return;
        }

        connections.unsubscribe(connectionId, id);
        String receipt = frame.getHeader("receipt");
        if (receipt != null) {
            sendReceiptFrame(receipt);
        }
    }

    private void send(StompFrame frame) {
        System.out.println("in StompMessagingProtocolImpl: send");
        String topic = frame.getHeader("destination");
        String body = frame.getBody();

        if (topic == null) {
            System.out.println("topic is null");
            sendErrorFrame(frame.getHeaders(), "Missing 'destination' header", "SEND frame must include a 'destination' header.");
            return;
        }
        if (body == null || body.isEmpty()) {
            System.out.println("body is empty");
            sendErrorFrame(frame.getHeaders(), "Missing message body", "SEND frame must include a non-empty body.");
            return;
        }

        if (connections.isSubscribed(connectionId, topic)) {
            sendMessageFrame(topic, body);
            String receipt = frame.getHeader("receipt");
            if (receipt != null) {
                sendReceiptFrame(receipt);
            }
        } else {
            System.out.println("user not sunscribed");
            sendErrorFrame(frame.getHeaders(), "User is not subscribed", "Only users who are subscribed to a 'topic' can send messages to all their subscribers.");
        }
    }

    private void disconnect(StompFrame frame) {
        System.out.println("in StompMessagingProtocolImpl: disconnect");
        shouldTerminate = true;
        connections.disconnect(connectionId);
        sendReceiptFrame(frame.getHeader("receipt"));
    }

    private void sendErrorFrame(Map<String, String> headers,String errorMessage, String detailedMessage) {
        System.out.println("in StompMessagingProtocolImpl: sendErrorFrame");
        headers.put("message", errorMessage);

        StompFrame errorFrame = new StompFrame(
            "ERROR", 
            headers, 
            detailedMessage
        );
        shouldTerminate = true;
        connections.send(connectionId, errorFrame);
    }

    private void sendMessageFrame(String topic, String detailedMessage) {
        System.out.println("in StompMessagingProtocolImpl: sendMessageFrame");
        Map<String, String> headers = new ConcurrentHashMap<>();
        headers.put("destination", topic);
        headers.put("message-id", "" + messageIdCounter.getAndIncrement());
        detailedMessage = "user: " + username + "\nchannel name: " + topic + "\n" + detailedMessage;
        StompFrame messageFrame = new StompFrame(
            "MESSAGE", 
            headers, 
            detailedMessage
        );
        connections.send(topic, messageFrame);
    }

    private void sendReceiptFrame(String receiptID) {
        System.out.println("in StompMessagingProtocolImpl: sendReceiptFrame");
        Map<String, String> headers = new ConcurrentHashMap<>();
        headers.put("receipt-id", receiptID);

        StompFrame recieptFrame = new StompFrame(
            "RECEIPT", 
            headers, 
            null
        );
        connections.send(connectionId, recieptFrame);
    }
}
