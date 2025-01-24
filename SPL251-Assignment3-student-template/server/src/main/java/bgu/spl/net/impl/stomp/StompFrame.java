package bgu.spl.net.impl.stomp;

import java.util.HashMap;
import java.util.Map;

public class StompFrame{
    private String command;
    private Map<String, String> headers;
    private String body;

    public StompFrame(String command, Map<String, String> headers, String body) {
        this.command = command;
        this. headers = headers != null ? headers : new HashMap<>();
        this.body = body;
    }

    public String getCommand() {
        return command;
    }

    public Map<String, String> getHeaders() {
        return headers;
    }

    public String getHeader(String key) {
        return headers.get(key);
    }

    public String getBody() {
        return body;
    }

    @Override
    public String toString() {
        StringBuilder builder = new StringBuilder();
        builder.append(command).append("\n");
        headers.forEach((key, value) -> builder.append(key).append(":").append(value).append("\n"));
        builder.append("\n");
        if (body != null && !body.isEmpty()) {
            builder.append(body);
        }
        builder.append("\0");
        return builder.toString();
    }
}
