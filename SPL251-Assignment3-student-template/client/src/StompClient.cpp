#include <iostream> // Use for input and output 
#include <ostream> 
#include <thread> // Provide support for multi-threading
#include <string> // Handle string manipulations
#include <sstream> // Used for parsing commands
#include <mutex>  // Synchronizing access to shared data between threads
#include <unordered_map> // For storing data in key value format
#include <condition_variable> //Coordinate beetween multiplie thread
#include "../include/ConnectionHandler.h" 
#include "../include/event.h"
#include "../include/StompProtocol.h"
#include <fstream>
#include <iomanip>

int main(int argc, char* argv[]) {
    std::mutex mutex;
    bool shouldTerminate = false;  // Flag to know when the program should terminate
    bool isLoggedIn = false;       // Flag to check if the user is logged in
    std::string loggedInUsername;   // for storing the username of the logged-in user
    std::unordered_map<std::string, std::string> subscriptionMap; // stores the channel name as the key and the subscription ID as the value.
    std::unordered_map<std::string, std::vector<Event>> eventsMap; //stores all events per channel
    std::condition_variable cv; // Condition variable for signaling. makes the thread wait till it is notified by the other thread.
    ConnectionHandler* connectionhandler = nullptr;
    StompProtocol* stompProtocol = nullptr;

    // Thread for server communication
    std::thread serverCommunicationThread;

    // (lambda) function to start/restart the server communication thread
    auto startServerCommunicationThread = [&]() {
        if (serverCommunicationThread.joinable()) {
            serverCommunicationThread.join();  // Ensure the previous thread is closed
        }

        serverCommunicationThread = std::thread([&]() {
            while (!shouldTerminate) {
                try {
                    std::string msg;

                    // Wait for a connection handler to be ready (valid) or shouldTerminate flag true
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        //std::cout << "DEBUG: Waiting on condition variable..." << std::endl;
                        cv.wait(lock, [&]() { 
                            //std::cout << "DEBUG: Condition evaluated." << std::endl;
                            return connectionhandler != nullptr || shouldTerminate; 
                        });
                        //std::cout << "DEBUG: Condition met, proceeding..." << std::endl;
                    }

                    if (shouldTerminate) {
                        break;
                    }

                    // Process messages from the server
                    if (connectionhandler && connectionhandler->getLine(msg)) {
                        //std::cout << "DEBUG: Received message from server: " << msg << std::endl;

                        if (msg.find("CONNECTED") == 0) {
                            std::lock_guard<std::mutex> lock(mutex);
                            isLoggedIn = true;
                            std::cout << "Login successful." << std::endl;
                        } else if (msg.find("ERROR") == 0) {
                            std::cerr << "Server ERROR: " << msg << std::endl;
                        } else if (msg.find("MESSAGE") == 0) {
                            std::cout << "Server MESSAGE: " << msg << std::endl;
                        } else if (msg.find("RECEIPT") == 0) {
                            std::cout << "Server RECEIPT: " << msg << std::endl;
                        }else {
                            stompProtocol->processServerMessage(msg);
                        }
                    } else {
                        std::cerr << "Connection to server lost." << std::endl;
                        //std::lock_guard<std::mutex> lock(mutex);
                        isLoggedIn = false;
                        //std::cout << "DEBUG: Notifying condition variable." << std::endl;
                        cv.notify_all();
                        break;  // Exit the thread if the connection is lost
                    }
                } catch (const std::exception& ex) {
                    std::cerr << "Exception in server communication thread: " << ex.what() << std::endl;
                    //std::lock_guard<std::mutex> lock(mutex);
                    shouldTerminate = true;
                    //std::cout << "DEBUG: Notifying condition variable." << std::endl;
                    cv.notify_all();
                    break;
                }
            }

            std::cout << "Server communication thread terminated." << std::endl;
        });
    };

    // Start the server communication thread
    startServerCommunicationThread();

    // Main loop for handling user input
    while (!shouldTerminate) {
        std::string userInput;
        std::getline(std::cin, userInput);

        if (userInput.rfind("login ", 0) == 0) {
            std::lock_guard<std::mutex> lock(mutex);

            if (isLoggedIn) {
                std::cerr << "You are already logged in. Please log out first." << std::endl;
                continue;
            }
            //Parse the login command 
            std::istringstream userInputStream(userInput);
            std::string command, hostPort, username, password;
            userInputStream >> command >> hostPort >> username >> password;
            //handeling host&port "127.0.0.1:7777" (example for template)
            auto colonPos = hostPort.find(':');
            if (colonPos == std::string::npos) {
                std::cerr << "Invalid login command. Usage: login <host:port> <username> <password>" << std::endl;
                continue;
            }

            std::string host = hostPort.substr(0, colonPos);
            int port = std::stoi(hostPort.substr(colonPos + 1)); //std::stoi - Convert string to integer

            connectionhandler = new ConnectionHandler(host, port);//For establish TCP Connection
            if (!connectionhandler->connect()) { // if connecrtion fails clean up resurces 
                delete connectionhandler;
                connectionhandler = nullptr;
                std::cerr << "Could not connect to server." << std::endl;
                continue;
            }

            stompProtocol = new StompProtocol(*connectionhandler);

            std::string connectFrame = stompProtocol->createConnectFrame(host, username, password);
            if (!connectionhandler->sendLine(connectFrame)) {
                std::cerr << "Failed to send CONNECT frame to server." << std::endl;
                delete connectionhandler;
                delete stompProtocol;
                connectionhandler = nullptr;
                stompProtocol = nullptr;
                continue;
            }

            loggedInUsername = username;
            std::cout << "Login request sent to server." << std::endl;
            //std::cout << "DEBUG: Notifying condition variable." << std::endl;
            cv.notify_all();
        }

        else if (userInput.rfind("join ", 0) == 0) {
            if (!isLoggedIn) {
                std::cerr << "You must be logged in to join a channel." << std::endl;
                continue;
            }

            std::string channelName = userInput.substr(5);
            if (channelName.empty()) {
                std::cerr << "Invalid join command. Usage: join <channel_name>" << std::endl;
                continue;
            }
            //Create unique id for this specific subscription using counter
            static int subscriptionId = 1;// static int exists even after we finished the func/scope, and keeps on the data beetwen callings to the func.
            std::string subscriptionIdStr = std::to_string(subscriptionId++); //increment the value AFTER converting it to string
            subscriptionMap[channelName] = subscriptionIdStr;

            std::string subscribeFrame = stompProtocol->createSubscribeFrame(channelName, subscriptionIdStr);
            connectionhandler->sendLine(subscribeFrame);
        }
        else if(userInput.rfind("exit ", 0) == 0){

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
                connectionhandler->sendLine(unsubscribeFrame);

        }

        else if (userInput.rfind("report ", 0) == 0) {
            if (!isLoggedIn) {
                std::cerr << "You must be logged in to send a report." << std::endl;
                continue;
            }

            std::string fileName = userInput.substr(7);
            if (fileName.empty()) {
                std::cerr << "Invalid report command. Usage: report <file_name>" << std::endl;
                continue;
            }

            try {
                names_and_events parsedData = parseEventsFile(fileName);

                for (Event& event : parsedData.events) {
                    event.setEventOwnerUser(loggedInUsername);
                    eventsMap[parsedData.channel_name].push_back(event);

                    std::ostringstream oss;
                    oss << "Event Name: " << event.get_name() << "\n"
                        << "Description: " << event.get_description() << "\n"
                        << "City: " << event.get_city() << "\n"
                        << "Date Time: " << event.get_date_time() << "\n"
                        << "General Information:\n";

                    for (const auto& pair : event.get_general_information()) {
                        oss << "  " << pair.first << ": " << pair.second << "\n";
                    }

                    std::string serializedEvent = oss.str();
                    std::string sendFrame = stompProtocol->createSendFrame(parsedData.channel_name, serializedEvent);
                    connectionhandler->sendLine(sendFrame);
                }

                //std::cout << "Report successfully sent for file: " << fileName << std::endl;
            } catch (const std::exception& ex) {
                std::cerr << "Failed to process report file '" << fileName << "': " << ex.what() << std::endl;
            }
        }



        else if(userInput.rfind("summary ") == 0){

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
                int counter_for_active = 0;
                int counter_for_forces_arrival_at_scene = 0;
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
                    outFile << "date time: " << epochToDate(event.get_date_time()) << "\n";
                    outFile << "event name: " << event.get_name() << "\n";
                    outFile << "summary: " << (event.get_description().length() > 27 ? event.get_description().substr(0, 27) + "..." : event.get_description()) << "\n";
                }
                outFile.close();
                continue;
        }

        else if (userInput == "logout") {
            if (!isLoggedIn) {
                std::cerr << "You must be logged in to log out." << std::endl;
                continue;
            }

            std::string disconnectFrame = stompProtocol->createDisconnectFrame();
            connectionhandler->sendLine(disconnectFrame);

            delete connectionhandler;
            delete stompProtocol;
            connectionhandler = nullptr;
            stompProtocol = nullptr;

            isLoggedIn = false;
            loggedInUsername.clear();
            subscriptionMap.clear();
            eventsMap.clear();

            std::cout << "Logout successful. You can log in again." << std::endl;

            // Restart the server communication thread for a new login session
            startServerCommunicationThread();
        }

        else if (userInput == "exit") {
            shouldTerminate = true;
            //std::cout << "DEBUG: Notifying condition variable." << std::endl;
            cv.notify_all();
        }
    }

    if (serverCommunicationThread.joinable()) {
        serverCommunicationThread.join();
    }

    std::cout << "Client terminated. Goodbye!" << std::endl;
    return 0;
}


std::string epochToDate(int epoch) {
    std::time_t time = epoch; // Convert to time_t
    std::tm tm = *std::localtime(&time); // Convert to local time structure

    std::ostringstream oss;
    oss << std::put_time(&tm, "%d/%m/%y %H:%M"); // Format as "DD/MM/YY HH:MM"
    return oss.str();
}