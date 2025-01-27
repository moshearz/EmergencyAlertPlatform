package bgu.spl.net.srv;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.impl.stomp.StompFrame;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.SocketChannel;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

public class NonBlockingConnectionHandler implements ConnectionHandler<StompFrame> {

    private static final int BUFFER_ALLOCATION_SIZE = 1 << 13; //8k
    private static final ConcurrentLinkedQueue<ByteBuffer> BUFFER_POOL = new ConcurrentLinkedQueue<>();

    private final StompMessagingProtocol<StompFrame> protocol;
    private final MessageEncoderDecoder<StompFrame> encdec;
    private final Queue<ByteBuffer> writeQueue = new ConcurrentLinkedQueue<>();
    private final SocketChannel chan;
    private final Reactor reactor;

    private volatile boolean initialized = false;
    private SelectionKey key;

    public NonBlockingConnectionHandler(
            MessageEncoderDecoder<StompFrame> reader,
            StompMessagingProtocol<StompFrame> protocol,
            SocketChannel chan,
            Reactor reactor) {
        this.chan = chan;
        this.encdec = reader;
        this.protocol = protocol;
        this.reactor = reactor;
    }

    public void setKey(SelectionKey key) {
        this.key = key;
    }

    public Runnable continueRead() {
        if (!initialized) return null;

        ByteBuffer buf = leaseBuffer();

        boolean success = false;
        try {
            success = chan.read(buf) != -1;
            if (success) {
                buf.flip();
                return () -> {
                    try {
                        while (buf.hasRemaining()) {
                            StompFrame nextMessage = encdec.decodeNextByte(buf.get());
                            if (nextMessage != null) {
                                protocol.process(nextMessage);
                                if (protocol.shouldTerminate()) {
                                    close();
                                    return;
                                }
                            }
                        }
                    } finally {
                        releaseBuffer(buf);
                    }
                };
            } else {
                releaseBuffer(buf);
                close();
            }
        } catch (IOException ex) {
            ex.printStackTrace();
        }
        return null;
    }

    public void close() {
        try {
            if (key != null) {
                key.cancel();
            }
            if (chan.isOpen()) {
                chan.close();   
            }
        } catch (IOException ex) {
            ex.printStackTrace();
        }
    }

    public boolean isClosed() {
        return !chan.isOpen();
    }

    public void continueWrite() {
        while (!writeQueue.isEmpty()) {
            try {
                ByteBuffer top = writeQueue.peek();
                chan.write(top);
                if (top.hasRemaining()) {
                    return;
                } else {
                    writeQueue.remove();
                }
            } catch (IOException ex) {
                ex.printStackTrace();
                close();
            }
        }

        if (writeQueue.isEmpty()) {
            if (protocol.shouldTerminate()) close();
            else reactor.updateInterestedOps(chan, SelectionKey.OP_READ);
        }
    }

    private static ByteBuffer leaseBuffer() {
        ByteBuffer buff = BUFFER_POOL.poll();
        if (buff == null) {
            return ByteBuffer.allocateDirect(BUFFER_ALLOCATION_SIZE);
        }

        buff.clear();
        return buff;
    }

    private static void releaseBuffer(ByteBuffer buff) {
        BUFFER_POOL.add(buff);
    }

    @Override
    public void send(StompFrame msg) {
        System.out.println("@@@@@@@@@@@@@@@@@@@@@@@@@NonBlockingConnectionHandler:send@@@@@@@@@@@@@@@@@@@@@@@@@@");
        ByteBuffer buffer = ByteBuffer.wrap(encdec.encode(msg));
        writeQueue.add(buffer);
        reactor.updateInterestedOps(chan, SelectionKey.OP_WRITE);
    }

    public void setInitialized() {
        this.initialized = true;
    }
}
