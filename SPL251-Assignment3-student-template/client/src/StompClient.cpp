#include <iostream> // Use for input and output 
#include <thread>   // Provide support for multi-threading
#include <string>   // Handle string manipulations
#include <sstream>  // Used for parsing commands
#include <mutex>    // Synchronizing access to shared data between threads
#include "../include/ConnectionHandler.h" // Manages the network connection between the server and the client
#include "../include/event.h" // Handles emergency events and their properties
#include "../include/StompProtocol.h" // Processes STOMP protocol commands like CONNECT, SEND, and SUBSCRIBE
#include <unordered_map> //for the map..
#include <fstream> // for file operations


int main(int argc, char *argv[]) {

    //First, waiting for loggin command because we only process commands from logged-in users.
    std::cout << "Waiting for login command, Structure: login {host:port} {username} {password}" << std::endl;

    //Shared resources for threads 1,2
    std::mutex mutex;
    bool shouldTerminate = false; // flag to know when the program should exist
    bool isLoggedIn = false; //flad to know if the user logged in
    std::string loggedInUsername;  // for storing the username of the logged-in user
    std::unordered_map<std::string, std::string> subscriptionMap; // stores the channel name as the key and the subscription ID as the value.
    std::unordered_map<std::string, std::vector<Event>> eventsMap; //stores all events per channel

    //shared resource for the commands that needs access to the channel ID and user name 


    ConnectionHandler* connectionhandler = nullptr;
    StompProtocol* stompProtocol = nullptr;


    //Thread 1 - Server Communication 
    //This thread should waits until user logged in, once it happaning this thread become active and starts listening for messages from the server
    std::thread serverCommunicationThread([&stompProtocol, &connectionhandler, &shouldTerminate, &mutex, &isLoggedIn]() {

        while (!shouldTerminate) {
            std::lock_guard<std::mutex> lock(mutex); // Lock the mutex

            if (!isLoggedIn) { // If not logged in, wait and avoid busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            std::string msg;
            if (!connectionhandler->getLine(msg)) { // Handle server communication
                std::cerr << "Error receiving message from server. Exiting..." << std::endl;
                shouldTerminate = true;
                break;
            }

            std::cout << "Message from the server: " << msg << std::endl;

            stompProtocol->processServerMessage(msg); // Pass the message to the STOMP Protocol for processing
        }
    });


    //Thread 2 - Keyboard Thread
    std::thread KeyboardThread([&stompProtocol, &connectionhandler, &shouldTerminate, &mutex, &isLoggedIn, &loggedInUsername, &subscriptionMap, &eventsMap]() {
        
        while(!shouldTerminate){

            std::string userInput;
            std::getline(std::cin, userInput); // waits for user input - we will use this input as classified all the commands

            //Login command
            if(userInput.rfind("login ", 0) == 0){

                std::lock_guard<std::mutex> lock(mutex); //Locks the shared resources so that no other thread can access them while this command is being processed.

                //Client already logged in
                if (connectionhandler != nullptr) { // Check if the client is already logged in, we will return the connection handler to be nullptr after every 
                    std::cerr << "The client is already logged in, log out before trying again" << std::endl;
                    return; // using return for exiting the entire login command scope
                }

                //Parse the login command 
                std::istringstream userInputStream(userInput);
                std::string command, hostPort, username, pwd;
                userInputStream >> command >> hostPort >> username >> pwd;

                //handeling host&port "127.0.0.1:7777" (example for template)
                auto colonPos = hostPort.find(':');
                if (colonPos == std::string::npos) {
                    std::cerr << "Invalid login command. Usage: login <host:port> <username> <password>" << std::endl;
                    continue;}
                std::string host = hostPort.substr(0, colonPos);
                int port = std::stoi(hostPort.substr(colonPos + 1)); //std::stoi - Convert string to integer

                //Connect to the server - Attempt
                 connectionhandler = new ConnectionHandler(host, port); // the "connectionhandler" we created before as nullptr
                 if(!connectionhandler->connect()){ // TCP Connection
                    delete connectionhandler;
                    connectionhandler = nullptr;
                    std::cerr << "Could not connect to server " << std::endl;
                    continue;
                 }
                 std::cerr << "Connected to server :) at " << host << ":" << port << std::endl;

                // Initialize STOMP protocol
                stompProtocol = new StompProtocol(*connectionhandler);

                // Send CONNECT frame to the server 
                std::string connectFrame = stompProtocol->createConnectFrame(host, username, pwd);
                if (!connectionhandler->sendLine(connectFrame)) { // Send the CONNECT frame
                    std::cerr << "Failed to send CONNECT frame to server." << std::endl;
                    delete connectionhandler;
                    delete stompProtocol;
                    connectionhandler = nullptr;
                    stompProtocol = nullptr;
                    continue;
                }

                   // Wait for server response
                std::string serverResponse;
                auto startTime = std::chrono::steady_clock::now();
                while (!connectionhandler->getLine(serverResponse)) {
                    auto elapsedTime = std::chrono::steady_clock::now() - startTime;
                    if (std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count() > 5) { // 5-second timeout
                        std::cerr << "Timeout: No response from server after sending CONNECT frame." << std::endl;
                        delete connectionhandler;
                        delete stompProtocol;
                        connectionhandler = nullptr;
                        stompProtocol = nullptr;
                        return;
                    }
                }

                if (serverResponse.find("CONNECTED") == 0) {
                    std::cout << "Login successful" << std::endl;
                    isLoggedIn = true;
                    loggedInUsername = username;
                    continue;
                } else if (serverResponse.find("ERROR") == 0) {
                    std::cerr << "Error: " << serverResponse << std::endl;
                    delete connectionhandler;
                    delete stompProtocol;
                    connectionhandler = nullptr;
                    stompProtocol = nullptr;
                    return;
                }

            }

            //join command
            if(userInput.rfind("join ", 0) == 0){

                std::lock_guard<std::mutex> lock(mutex);

                if(!isLoggedIn){
                    std::cerr << "User must be logged in before joining to channel. " << std::endl;
                    continue;
                }
                
                //Parse the channel name (in here we will use in substring instead of creating a stream because its short command and simple template)
                std::string channel_name = userInput.substr(5); // join is 5 chatacters including the space. Structure: join {channel_name}
                if(channel_name.empty()){
                    std::cerr << "Invalid join command. Structure: join {channel_name}" << std::endl;
                    continue;
                }

                //Create unique id for this specific subscription using counter. note to self: first id = 1
                static int subscribeId = 1; // static int exists even after we finished the func/scope, and keeps on the data beetwen callings to the func.
                std::string subscribeId_str = std::to_string(subscribeId++); //increment the value AFTER converting it to string 
                subscriptionMap[channel_name] = subscribeId_str;
                
                // Create SUBSCRIBE Frame using StompProtocol
                std::string subscribeFrame = stompProtocol->createSubscribeFrame(channel_name, subscribeId_str);

                //send the subscribe frame to the server
                if(!connectionhandler->sendLine(subscribeFrame)){
                    std::cerr << "Failed to send SUBSCRIBE frame to the server from channel: " << channel_name << std::endl;
                    continue;
                }

                // Wait for the RECEIPT frame from the server
                std::string serverResponse;
                if (!connectionhandler->getLine(serverResponse)) {
                    std::cerr << "No response from server after sending SUBSCRIBE frame." << std::endl;
                    continue;
                }

                // Check if the response is a RECEIPT frame
                if(serverResponse.find("RECEIPT") == 0) {
                    std::cout << "Joined channel " << channel_name << std::endl;
                } else {
                    std::cerr << "Unexpected response from server: " << serverResponse << std::endl;
                }
            }

            //exit command
            if(userInput.rfind("exit ", 0) == 0){

                std::lock_guard<std::mutex> lock(mutex);
                if(!isLoggedIn){
                    std::cerr << "User must be logged in before exiting from channel. " << std::endl;
                    continue;
                }

                //Parse the channel name
                std::string channel_name = userInput.substr(5); // join is 5 chatacters including the space. Structure: join {channel_name}
                if(channel_name.empty()){
                    std::cerr << "Invalid join command. Structure: join {channel_name}" << std::endl;
                    continue;
                }

                //check if the given user is assigned to the channel
                auto it = subscriptionMap.find(channel_name);
                if(it == subscriptionMap.end()){ // checks if the channel found in the map
                    std::cerr << "you are not subscribed to channel: " << channel_name << std::endl;
                    continue;
                }

                //create UNSUBSCRIBE frame using StompProtocol
                std::string unsubscribeFrame = stompProtocol->createUnsubscribeFrame(it->second); //second accesses the subscription ID that associated with channel_name

                //send the unsubscribe frame to the server
                if(!connectionhandler->sendLine(unsubscribeFrame)){
                    std::cerr << "Failed to send UNSUBSCRIBE frame to the server from channel: " << channel_name << std::endl;
                    continue;
                }

                // Wait for the RECEIPT frame from the server
                std::string serverResponse;
                if (!connectionhandler->getLine(serverResponse)) {
                    std::cerr << "No response from server after sending UNSUBSCRIBE frame." << std::endl;
                continue;
                }

                // Check if the response is a RECEIPT frame
                if(serverResponse.find("RECEIPT") == 0) {
                    std::cout << "Exited channel " << channel_name << std::endl;
                    subscriptionMap.erase(channel_name);
                } else {
                    std::cerr << "Unexpected response from server: " << serverResponse << std::endl;
                }
                continue;
            }

            //report command
            if(userInput.rfind("report ", 0) == 0){

                std::lock_guard<std::mutex> lock(mutex);
                if(!isLoggedIn){
                    std::cerr << "User must be logged in for sending a report... " << std::endl;
                    continue;
                }

                //Extract the file name
                std::string file_name = userInput.substr(7); //Structure: report {file}
                if (file_name.empty()) {
                    std::cerr << "Invalid report command. Usage: report {file}" << std::endl;
                    continue;
                }
                //PARSE THE CHANNEL NAME AND EVENTS IT CONTAINS 
                names_and_events parsedData_names_events;
                parsedData_names_events = parseEventsFile(file_name); //returns a names_and_events object

                //add the parsed events to eventsMap, we will use this is the "summary" command, when we need to create the report 
                for(const Event& event : parsedData_names_events.events) eventsMap[parsedData_names_events.channel_name].push_back(event);

                for (const Event& event : parsedData_names_events.events) { //create the send frame
                    std::string sendFrame = "SEND\n"
                                            "destination:/" + parsedData_names_events.channel_name + "\n"
                                            "user:" + loggedInUsername + "\n"
                                            "city:" + event.get_city() + "\n"
                                            "event name:" + event.get_name() + "\n"
                                            "date time:" + std::to_string(event.get_date_time()) + "\n"
                                            "general information:\n"
                                            "active:" + (event.get_general_information().at("active") == "true" ? "true" : "false") + "\n"
                                            "forces_arrival_at_scene:" + (event.get_general_information().at("forces_arrival_at_scene") == "true" ? "true" : "false") + "\n"
                                            "description:\n" + event.get_description() + "\n\n\0";
                    connectionhandler->sendLine(sendFrame);
                }
                continue;
            }

            if(userInput.rfind("summary ") == 0){

                std::lock_guard<std::mutex> lock(mutex);
                if(!isLoggedIn){
                    std::cout << "User must be logged in for doing a summary..." << std::endl;
                    continue;
                }

                //Parse the summary command
                std::istringstream userInputStreammm(userInput);
                std::string command, channel_name, user, file_name; // Structure: summary {channel_name} {user} {file}
                userInputStreammm >> command >> channel_name >> user >> file_name;

                auto it = eventsMap.find(channel_name);
                if (it == eventsMap.end()) {
                    std::cerr << "No events found for channel: " << channel_name << std::endl;
                    continue;
                }   
                const std::vector<Event>& events = it->second; // all events of specified channel_name. note: type is: std::vector<Event>
                std::vector<Event> userEvents; //for storing only the events of specific user

                for (const Event& event : events){
                    if(event.getEventOwnerUser() == user)
                        userEvents.push_back(event); //add the cuur event to the userEvents vector
                }

                // Sort events by date_time, then by event_name
                std::sort(userEvents.begin(), userEvents.end(), [](const Event& a, const Event& b) {
                    if (a.get_date_time() != b.get_date_time()) return a.get_date_time() < b.get_date_time();
                    return a.get_name() < b.get_name();
                });

                std::ofstream outFile(file_name); //open the file for writing

                int totalReports = userEvents.size();
                int counter_for_active, counter_for_forces_arrival_at_scene = 0;
                for (const Event& event : userEvents){
                    if(event.get_general_information().at("active")=="true") counter_for_active++;
                    if(event.get_general_information().at("forces_arrival_at_scene")=="true") counter_for_forces_arrival_at_scene++;
                }

                outFile << "Channel " << channel_name << "\n"; //write new headers or replace existing ones
                outFile << "States:\n"; //""
                outFile << "Total: " << totalReports << "\n";
                outFile << "active: " << counter_for_active << "\n";
                outFile << "forces_arrival_at_scene: " << counter_for_forces_arrival_at_scene << "\n";
                outFile << "Event Reports:\n";
                int the_num_of_report = 1; //starts from 1..
                for (const Event& event : userEvents){ // we sorted the events earlier so the first event will be asscieted to report 1 and so on... 
                    outFile << "Report_" << the_num_of_report++ << ":\n"; //Post add of the num of report
                    outFile << "city: " << event.get_city() << "\n";
                    outFile << "date time: " << event.get_date_time() << "\n";
                    outFile << "event name: " << event.get_name() << "\n";
                    outFile << "summary: " << (event.get_description().length() > 27 ? event.get_description().substr(0, 27) + "..." : event.get_description()) << "\n";
                }
                outFile.close();
                continue;
            }

            if(userInput == "logout") {
                std::lock_guard<std::mutex> lock(mutex);
                if(!isLoggedIn){
                    std::cout << "User must be logged in for log out haha..." << std::endl;
                    continue;
                }
                std::string disconnectFrame = stompProtocol->createDisconnectFrame();
                if(!connectionhandler->sendLine(disconnectFrame)) std::cerr << "Failed to send DISCONNECT frame to server." << std::endl;

                std::string serverResponse;
                if (!connectionhandler->getLine(serverResponse)) {
                    std::cerr << "No response from server after sending DISCONNECT frame." << std::endl;
                    continue;
                }                   

                // Check if the response is a RECEIPT frame
                if (serverResponse.find("RECEIPT") == 0) {
                std::cout << "Logout successful. Server acknowledged DISCONNECT." << std::endl;
                } else {
                    std::cerr << "Unexpected response from server: " << serverResponse << std::endl;
                }

                // Clean up resources and Reset maps and flags and the userr
                delete connectionhandler;
                delete stompProtocol;
                connectionhandler = nullptr;
                stompProtocol = nullptr;
                isLoggedIn = false;
                loggedInUsername.clear();
                subscriptionMap.clear();
                eventsMap.clear();
            }

        }
    });

    KeyboardThread.join();
    serverCommunicationThread.join();
    std::cout << "Client terminated !. Please come back again :)" << std::endl;
    return 0;
}