package bgu.spl.net.srv;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.impl.stomp.StompFrame;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.net.Socket;

public class BlockingConnectionHandler implements Runnable, ConnectionHandler<StompFrame> {

    private final StompMessagingProtocol<StompFrame> protocol;
    private final MessageEncoderDecoder<StompFrame> encdec;
    private final Socket sock;
    private BufferedInputStream in;
    private volatile boolean connected = true;

    public BlockingConnectionHandler(Socket sock, MessageEncoderDecoder<StompFrame> reader, StompMessagingProtocol<StompFrame> protocol) {
        this.sock = sock;
        this.encdec = reader;
        this.protocol = protocol;
    }

    @Override
    public void run() {
        try (Socket sock = this.sock) { //just for automatic closing
            int read;

            in = new BufferedInputStream(sock.getInputStream());
            
            while (!protocol.shouldTerminate() && connected && (read = in.read()) >= 0) {
                StompFrame nextMessage = encdec.decodeNextByte((byte) read);
                //System.out.println("BlockingCH: nextMessage = " + nextMessage);
                if (nextMessage != null) {
                    protocol.process(nextMessage);
                    if (protocol.shouldTerminate()) {
                        close();
                        break;
                    }
                }
            }
        } catch (IOException ex) {
            ex.printStackTrace();
        }
    }

    @Override
    public void close() throws IOException {
        connected = false;
        
        sock.close();
    }

    @Override
    public void send(StompFrame msg) {
        try {
            byte[] encodedMsg = encdec.encode(msg);
            sock.getOutputStream().write(encodedMsg);
            sock.getOutputStream().flush();
            System.out.println("Attempting to send to client");
        } catch (IOException e) {
            System.err.println("Error Sending message to client: " + e.getMessage());
            e.printStackTrace();
            connected = false;
        }
    }
}
