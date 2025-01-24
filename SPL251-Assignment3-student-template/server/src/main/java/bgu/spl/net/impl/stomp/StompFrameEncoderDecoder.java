package bgu.spl.net.impl.stomp;

import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

import bgu.spl.net.api.MessageEncoderDecoder;

public class StompFrameEncoderDecoder implements MessageEncoderDecoder<StompFrame>{
    private StringBuilder buffer = new StringBuilder();

    @Override
    public StompFrame decodeNextByte(byte nextByte) {
        //System.out.println("EncoderDecoder: decodeNextByte = " + nextByte);
        buffer.append((char) nextByte);
        //System.out.println("Buffer: " + buffer.toString());
        //System.out.println("Last received char: " + (char) nextByte);
        if (buffer.toString().endsWith("\0")) {
            String rawFrame = buffer.toString();
            System.out.println("-------StompFrame-------");
            System.out.println(rawFrame);
            System.out.println("------------------------");
            buffer.setLength(0);
            return decode(rawFrame);
        }
        return null;
    }

    @Override
    public byte[] encode(StompFrame message) {
        return encodeFrame(message).getBytes(StandardCharsets.UTF_8);
    }

    private StompFrame decode(String rawFrame) {
        String[] parts = rawFrame.split("\n\n", 2);
        String headerPart = parts[0];
        String bodyPart = parts.length > 1 ? parts[1] : "";
        String[] lines = headerPart.split("\n");

        String command = lines[0];

        Map<String, String> headers = new HashMap<>();
        for (int i = 1; i < lines.length; i++) {
            String[] header = lines[i].split(":", 2);
            if (header.length == 2) {
                headers.put(header[0], header[1]);
            }
        }

        bodyPart = bodyPart.endsWith("\0") ? bodyPart.substring(0, bodyPart.length() - 1) : bodyPart;

        return new StompFrame(command, headers, bodyPart);
    }

    private String encodeFrame(StompFrame frame) {
        return frame.toString();
    }
}
