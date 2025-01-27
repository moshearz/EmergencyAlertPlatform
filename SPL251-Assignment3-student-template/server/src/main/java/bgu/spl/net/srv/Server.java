package bgu.spl.net.srv;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.impl.stomp.StompFrame;

import java.io.Closeable;
import java.util.function.Supplier;

public interface Server extends Closeable {

    /**
     * The main loop of the server, Starts listening and handling new clients.
     */
    void serve();

    /**
     *This function returns a new instance of a thread per client pattern server
     * @param port The port for the server socket
     * @param protocolFactory A factory that creats new MessagingProtocols
     * @param encoderDecoderFactory A factory that creats new MessageEncoderDecoder
     * @param <T> The Message Object for the protocol
     * @return A new Thread per client server
     */
    public static Server  threadPerClient(
            int port,
            Supplier<StompMessagingProtocol<StompFrame> > protocolFactory,
            Supplier<MessageEncoderDecoder<StompFrame> > encoderDecoderFactory) {

        return new BaseServer(port, protocolFactory, encoderDecoderFactory) {
            @Override
            protected void execute(BlockingConnectionHandler  handler) {
                System.out.println("TPC Server Start");
                new Thread(handler).start();
            }
        };

    }

    /**
     * This function returns a new instance of a reactor pattern server
     * @param nthreads Number of threads available for protocol processing
     * @param port The port for the server socket
     * @param protocolFactory A factory that creats new MessagingProtocols
     * @param encoderDecoderFactory A factory that creats new MessageEncoderDecoder
     * @param <T> The Message Object for the protocol
     * @return A new reactor server
     */
    public static Server reactor(
            int nthreads,
            int port,
            Supplier<StompMessagingProtocol<StompFrame>> protocolFactory,
            Supplier<MessageEncoderDecoder<StompFrame>> encoderDecoderFactory) {
        return new Reactor(nthreads, port, protocolFactory, encoderDecoderFactory);
    }

}
