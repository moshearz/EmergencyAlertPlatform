package bgu.spl.net.srv;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.impl.stomp.StompFrame;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.function.Supplier;

public abstract class BaseServer implements Server {

    private final int port;
    private final Supplier<StompMessagingProtocol<StompFrame>> protocolFactory;
    private final Supplier<MessageEncoderDecoder<StompFrame>> encdecFactory;
    private final ConnectionsImpl connections;
    private ServerSocket sock;

    public BaseServer(
            int port,
            Supplier<StompMessagingProtocol<StompFrame>> protocolFactory,
            Supplier<MessageEncoderDecoder<StompFrame>> encdecFactory) 
    {

        this.port = port;
        this.protocolFactory = protocolFactory;
        this.encdecFactory = encdecFactory;
        this.connections = new ConnectionsImpl();
		this.sock = null;
    }

    @Override
    public void serve() {

        try (ServerSocket serverSock = new ServerSocket(port)) {
			System.out.println("Server started");

            this.sock = serverSock; //just to be able to close
            

            while (!Thread.currentThread().isInterrupted()) {
                Socket clientSock = serverSock.accept();
                StompMessagingProtocol<StompFrame> protocol = protocolFactory.get();
                BlockingConnectionHandler handler = new BlockingConnectionHandler(
                    clientSock,
                    encdecFactory.get(),
                    protocol
                );
                int connectionId = connections.connect(handler);
                protocol.start(connectionId, connections);
                execute(handler);
            }
        } catch (IOException ex) {
        }

        System.out.println("server closed!!!");
    }

    @Override
    public void close() throws IOException {
		if (sock != null)
			sock.close();
    }

    protected abstract void execute(BlockingConnectionHandler  handler);
}
