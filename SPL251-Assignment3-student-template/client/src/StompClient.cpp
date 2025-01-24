#include <iostream> // Use for input and output 
#include <thread> //provide support for multi threading
#include <string> //handeling thread manipulations
#include <sstream> //used for parsing (example extracting parts of logim command)
#include <mutex> // sychroizing access to shared data between threads 
#include "../include/ConnectionHandler.h" //manage the network connection between the server and the client 
#include "../include/event.h" // handels emergency events and their properties
#include "../include/StompProtocol.h" // Proces stomp protocol commands like conncet send and subscribe 



int main(int argc, char *argv[]) { // c- num of command line arfumanets passed. v - Array of strings containig the arguments 
	if (argc < 3){ // min num of arguments is program name, host, port
		std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
		return -1;
	}
	
	std::string host = argv[1]; //Extract port
	short port = std::stoi(argv[2]); //Extract  port (casting it)

	ConnectionHandler connectionHandler(host, port); //Create Connection handler to manage connection from the server

	if(!connectionHandler.connect()){
		std::cerr << "Failed to connect to server at " << host << ":" << port << std::endl;
    	return -1;
	}

	std::cout << "Connected to the server!" << std::endl;

	bool shouldTerminate = false; //A shared flag to control when both threads should stop.
	std::mutex mutex;

	//THREAD 1 : HANDLE SERVER COMMUNICATION
	std::thread serverCommunicationThread([&connectionHandler, &shouldTerminate, &mutex]() { //thread to continuously read messages from the server
    while (!shouldTerminate) {
        std::string msg;
        if (!connectionHandler.getLine(msg)) { //reads a msg from the server, if the connection fails..
            std::cerr << "Error receiving message from server. Exiting..." << std::endl;
            std::lock_guard<std::mutex> lock(mutex); // locks the mutex while updating the flag for thread safety
            shouldTerminate = true;
            break;
        }

        std::cout << "Message from server: " << msg << std::endl; //allowing the client see what the server sent to the client 

        if (msg.find("DISCONNECT") != std::string::npos) {
            std::lock_guard<std::mutex> lock(mutex); //Locks the mutex while updating shouldTerminate to ensure thread safety
            shouldTerminate = true;
        }
    }
	});

	//THREAD 2: HANDLES KEYBOARD INPUT
	std::thread keyboardThread([&connectionHandler, &shouldTerminate, &mutex](){ // thead to handle user input from the keyboard
		while(!shouldTerminate){
			//Read user input
			std::string userInput; // Stores the user command 
			std::getline(std::cin, userInput); //Reads a line of text ebtered by the user from the keyboard

			//Handle the login command
			if (userInput.rfind("login ", 0) == 0) { //check if the string-input starts with the string "login"
				std::cerr << "InValid login command!" << std::endl;
				//Parsing the login command 
				std::istringstream inputStringStreammm(userInput); //ccreates a stream to break the user input (string) into parts
				std::string command , hostPort, username, password; //example of the template: "login 127.0.0.1:7777 omri 123". login == command, 127.0.0.1:7777 == hostPort, omri == username, 123 == password
				inputStringStreammm >> command >> hostPort >> username >> password; 
			
				//splitting hostPort
				auto colonPos = hostPort.find(":"); // Serches for the colon ":" which seperator beetween host and port.
				if (colonPos == std::string::npos){
					std::cerr << "Invalid login command. Usage: login <host:port> <username> <password>" << std::endl;
					continue;
				};
				std::string host = hostPort.substr(0, colonPos);
				std::string post = hostPort.substr(colonPos + 1);

				std::string connectFrame = "CONNECT\n"
											"accept-version:1.2\n"
                                            "host:" + host + "\n"
                                            "login:" + username + "\n"
                                            "passcode:" + password + "\n\n"
                                            "\0";
				if (!connectionHandler.sendLine(connectFrame)) {
                    std::cerr << "Failed to send CONNECT frame to server." << std::endl;
                    std::lock_guard<std::mutex> lock(mutex);
                    shouldTerminate = true;
                } else {
                    std::cout << "Sent CONNECT frame." << std::endl;
                }
                continue;	
			}

			//Handle the logout command
			if(userInput == "logout"){
				std::lock_guard<std::mutex> lock(mutex);
                shouldTerminate = true;
                connectionHandler.sendLine("DISCONNECT");
                continue;
			}

			//Handle send Command
			if (userInput.rfind("send ", 0) == 0){
				size_t firstSpace = userInput.find(' ', 5);
				if (firstSpace == std::string::npos) { // checks if we got "space"
    			std::cerr << "Invalid send command. Usage: send <destination> <message>" << std::endl;
    			continue;
				}
				std::string destination = userInput.substr(5, firstSpace - 5); //// Extract destination
				std::string message = userInput.substr(firstSpace + 1); // Extract msg

				std::string sendFrame = "SEND\n"
                    					"destination:" + destination + "\n\n" +
                                        message + "\n\0";
				if (!connectionHandler.sendLine(sendFrame)) {
                    std::cerr << "Failed to send message to server." << std::endl;
                    std::lock_guard<std::mutex> lock(mutex);
                    shouldTerminate = true;
                }
                continue;
			}

			// Handle subscribe command
            if (userInput.rfind("subscribe ", 0) == 0) {
                std::istringstream inputStringStreammm(userInput);
                std::string command, destination, id;
                inputStringStreammm >> command >> destination >> id;

                std::string subscribeFrame = "SUBSCRIBE\n"
                                             "destination:" + destination + "\n"
                                             "id:" + id + "\n\n" //להוסיף תיקון לכך שיכול להיות המון שורות 
                                             "\0";

                if (!connectionHandler.sendLine(subscribeFrame)) {
                    std::cerr << "Failed to send SUBSCRIBE frame to server." << std::endl;
                    std::lock_guard<std::mutex> lock(mutex);
                    shouldTerminate = true;
                }
                continue;
            }

			// Handle unsubscribe command
            if (userInput.rfind("unsubscribe ", 0) == 0) {
                std::istringstream inputStringStreammm(userInput);
                std::string command, id;
                inputStringStreammm >> command >> id;

                std::string unsubscribeFrame = "UNSUBSCRIBE\n"
                                               "id:" + id + "\n\n"
                                               "\0";

                if (!connectionHandler.sendLine(unsubscribeFrame)) {
                    std::cerr << "Failed to send UNSUBSCRIBE frame to server." << std::endl;
                    std::lock_guard<std::mutex> lock(mutex);
                    shouldTerminate = true;
                }
                continue;
				
            }

			if (message.find("ERROR") == 0) {//derine Message 
            std::cerr << "Error received from server: " << message << std::endl;

            // Lock the mutex and signal termination
            {
                std::lock_guard<std::mutex> lock(mutex);
                shouldTerminate = true;
            }

            connectionHandler.close(); // Close the connection
            break;
        }

        protocol.handleServerMessage(message); // Process other server messages
    }



			// Handle unknown commands
            std::cerr << "Unknown command: " << userInput << std::endl;
		}
		// Join threads
    	std::thread serverCommunicationThread.join();
    	std::thread keyboardThread.join();

    	std::cout << "Client terminated gracefully." << std::endl;
    	return 0;
	});

 
	

}

