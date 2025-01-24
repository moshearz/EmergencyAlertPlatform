package bgu.spl.net.impl.stomp;

import java.util.HashMap;
import java.util.Map;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;

public class StompMessagingProtocolImpl  implements StompMessagingProtocol<StompFrame> {
    private int connectionId;
    private Connections<StompFrame> connections;
    private boolean shouldTerminate = false;

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
            case "SEND":
                send(frame);
                break;
            case "DISCONNECT":
                disconnect(frame);
                break;
            default:
                sendErrorFrame("Unknown command", "The command '" + command + "' isn't legal.");
                break;
        }
    }

    private void connect(StompFrame frame) {
        System.out.println("in StompMessagingProtocolImpl: connect");
        if (frame.getHeader("accept-version") == null) {
            sendErrorFrame("Missing 'accept-version' header", "CONNECT frame must include an 'accept-version' header");
            return;
        }

        Map<String, String> headers = new HashMap<>();
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
            sendErrorFrame("Missing 'destination' header", "SUBSCRIBE frame must include a 'destination' header.");
            return;
        }
        if (id == null) {
            sendErrorFrame("Missing 'id' header", "SUBSCRIBE frame must include an 'id' header.");
            return;
        }
        
        connections.subscribe(connectionId, topic);
    }

    private void send(StompFrame frame) {
        System.out.println("in StompMessagingProtocolImpl: send");
        String topic = frame.getHeader("destination");
        String body = frame.getBody();

        if (topic == null) {
            sendErrorFrame("Missing 'destination' header", "SEND frame must include a 'destination' header.");
            return;
        }
        if (body == null || body.isEmpty()) {
            sendErrorFrame("Missing message body", "SEND frame must include a non-empty body.");
            return;
        }

        connections.send(topic, frame);
    }

    private void disconnect(StompFrame frame) {
        System.out.println("in StompMessagingProtocolImpl: disconnect");
        shouldTerminate = true;
        connections.disconnect(connectionId);
    }

    private void sendErrorFrame(String errorMessage, String detailedMessage) {
        System.out.println("in StompMessagingProtocolImpl: sendErrorFrame");
        Map<String, String> headers = new HashMap<>();
        headers.put("message", errorMessage);

        StompFrame errorFrame = new StompFrame(
            "ERROR", 
            headers, 
            detailedMessage
        );
        connections.send(connectionId, errorFrame);
        shouldTerminate = true;
    }

    @Override
    public boolean shouldTerminate() {
        //System.out.println("in StompMessagingProtocolImpl: shouldTerminate");
        return shouldTerminate;
    }
}
