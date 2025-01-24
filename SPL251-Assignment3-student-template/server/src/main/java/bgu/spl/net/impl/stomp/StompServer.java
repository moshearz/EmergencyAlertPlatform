<<<<<<< HEAD
package bgu.spl.net.impl.stomp;

public class StompServer {

    public static void main(String[] args) {
        // TODO: implement this
    }
}
=======
package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.MessageEncoderDecoder;
import bgu.spl.net.srv.Server;

public class StompServer {

    public static void main(String[] args) {
        int port = Integer.parseInt(args[0]);
        String serverType = args[1];

        System.out.println("test");
        if (serverType.equals("tpc")) {
            Server.threadPerClient(
                port, 
                () -> new StompMessagingProtocolImpl(), 
                () -> new StompFrameEncoderDecoder()
            ).serve();
        } else if (serverType.equals("reactor")) {
            Server.reactor(
                Runtime.getRuntime().availableProcessors(), 
                port, 
                () -> new StompMessagingProtocolImpl(), 
                () -> new StompFrameEncoderDecoder()
            ).serve();
        } else {
            System.out.println("Invalid server type. Acceptable types: 'tpc' , 'reactor'");
        }
    }
}
>>>>>>> dc50916... Implemented main server function
