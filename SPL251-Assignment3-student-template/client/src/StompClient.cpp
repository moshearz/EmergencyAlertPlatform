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
	
	