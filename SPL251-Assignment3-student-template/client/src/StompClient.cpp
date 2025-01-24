#include <iostream> // Use for input and output
#include <thread>   // Provide support for multi-threading
#include <string>   // Handle string manipulations
#include <sstream>  // Used for parsing commands
#include <mutex>    // Synchronizing access to shared data between threads
#include "../include/ConnectionHandler.h" // Manages the network connection between the server and the client
#include "../include/event.h" // Handles emergency events and their properties
#include "../include/StompProtocol.h" // Processes STOMP protocol commands like CONNECT, SEND, and SUBSCRIBE

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
        return -1;
    }

    std::string host = argv[1];
    short port = std::stoi(argv[2]);

    ConnectionHandler connectionHandler(host, port);
    if (!connectionHandler.connect()) {
        std::cerr << "Failed to connect to server at " << host << ":" << port << std::endl;
        return -1;
    }

    std::cout << "Connected to the server!" << std::endl;

    // Shared resources
    bool shouldTerminate = false;
    std::mutex mutex;

    // Create the StompProtocol instance
    StompProtocol protocol(connectionHandler);

    // THREAD 1: Handle server communication
    std::thread serverCommunicationThread([&protocol, &connectionHandler, &shouldTerminate, &mutex]() {
        while (!shouldTerminate) {
            std::string msg;
            if (!connectionHandler.getLine(msg)) {
                std::cerr << "Error receiving message from server. Exiting..." << std::endl;
                std::lock_guard<std::mutex> lock(mutex);
                shouldTerminate = true;
                break;
            }

            // Handle multi-line messages
            while (msg.back() != '\0') {
                std::string nextLine;
                if (!connectionHandler.getLine(nextLine)) {
                    std::cerr << "Error receiving multi-line message from server." << std::endl;
                    std::lock_guard<std::mutex> lock(mutex);
                    shouldTerminate = true;
                    break;
                }
                msg += nextLine;
            }

            // Remove null terminator for display
            if (!msg.empty() && msg.back() == '\0') {
                msg.pop_back();
            }

            std::cout << "Message from server:\n" << msg << std::endl;

            if (msg.find("DISCONNECT") == 0) {
                std::cerr << "Received DISCONNECT or ERROR frame. Terminating client..." << std::endl;
                std::lock_guard<std::mutex> lock(mutex);
                shouldTerminate = true;
                connectionHandler.close();
                break;
            }
            if (msg.find("EROR") == 0) {
                std::cerr << "Received EROR frame. Terminating client..." << std::endl;
                std::lock_guard<std::mutex> lock(mutex);
                shouldTerminate = true;
                connectionHandler.close();
                break;
            }
            protocol.processServerMessage(msg);

        }
    });

    // THREAD 2: Handle keyboard input
std::thread keyboardThread([&protocol, &connectionHandler, &shouldTerminate, &mutex]() {
        while (!shouldTerminate) {
            std::string userInput;
            std::getline(std::cin, userInput);

            // Handle the login command
            if (userInput.rfind("login ", 0) == 0) {
                std::istringstream userInputStreamm(userInput);
                std::string command, hostPort, username, password;
                userInputStreamm >> command >> hostPort >> username >> password;

                auto colonPos = hostPort.find(':');
                if (colonPos == std::string::npos) {
                    std::cerr << "Invalid login command. Usage: login <host:port> <username> <password>" << std::endl;
                    continue;
                }

                std::string host = hostPort.substr(0, colonPos);
                std::string portStr = hostPort.substr(colonPos + 1);

                std::string connectFrame = protocol.createConnectFrame(host, username, password);
                if (!connectionHandler.sendLine(connectFrame)) {
                    std::cerr << "Failed to send CONNECT frame to server." << std::endl;
                    std::lock_guard<std::mutex> lock(mutex);
                    shouldTerminate = true;
                }
                continue;
            }

            // Handle the logout command
            if (userInput == "logout") {
                std::string disconnectFrame = protocol.createDisconnectFrame();
                if (!connectionHandler.sendLine(disconnectFrame)) {
                    std::cerr << "Failed to send DISCONNECT frame to server." << std::endl;
                }
                std::lock_guard<std::mutex> lock(mutex);
                shouldTerminate = true;
                continue;
            }

            // Handle send command
            if (userInput.rfind("send ", 0) == 0) {
                size_t firstSpace = userInput.find(' ', 5);
                if (firstSpace == std::string::npos) {
                    std::cerr << "Invalid send command. Usage: send <destination> <message>" << std::endl;
                    continue;
                }

                std::string destination = userInput.substr(5, firstSpace - 5);
                std::string message = userInput.substr(firstSpace + 1);

                std::string sendFrame = protocol.createSendFrame(destination, message);
                if (!connectionHandler.sendLine(sendFrame)) {
                    std::cerr << "Failed to send SEND frame to server." << std::endl;
                    std::lock_guard<std::mutex> lock(mutex);
                    shouldTerminate = true;
                }
                continue;
            }

            // Handle subscribe command
            if (userInput.rfind("subscribe ", 0) == 0) {
                std::istringstream iss(userInput);
                std::string command, destination, id;
                iss >> command >> destination >> id;

                std::string subscribeFrame = protocol.createSubscribeFrame(destination, id);
                if (!connectionHandler.sendLine(subscribeFrame)) {
                    std::cerr << "Failed to send SUBSCRIBE frame to server." << std::endl;
                    std::lock_guard<std::mutex> lock(mutex);
                    shouldTerminate = true;
                }
                continue;
            }

            // Handle unsubscribe command
            if (userInput.rfind("unsubscribe ", 0) == 0) {
                std::istringstream iss(userInput);
                std::string command, id;
                iss >> command >> id;

                std::string unsubscribeFrame = protocol.createUnsubscribeFrame(id);
                if (!connectionHandler.sendLine(unsubscribeFrame)) {
                    std::cerr << "Failed to send UNSUBSCRIBE frame to server." << std::endl;
                    std::lock_guard<std::mutex> lock(mutex);
                    shouldTerminate = true;
                }
                continue;
            }

            std::cerr << "Unknown command: " << userInput << std::endl;
        }
    });

    // Join threads
    serverCommunicationThread.join();
    keyboardThread.join();

    std::cout << "Client terminated gracefully." << std::endl;
    return 0;
}
