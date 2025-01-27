#include "../include/StompProtocol.h"
#include <iostream>
#include <sstream>

StompProtocol::StompProtocol(ConnectionHandler &handler) : connectionHandler(handler), subscriptions(), receiptCounter(0) {}

std::string StompProtocol::createConnectFrame(const std::string &host, const std::string &username, const std::string &password){
    return "CONNECT\n"
        "accept-version:1.2\n"
        "host:" + host + "\n"
        "login:" + username + "\n"
        "passcode:" + password + "\n\n\0";
}

std::string StompProtocol::createSendFrame(const std::string &destination, const std::string &message)
{
    return "SEND\n"
           "destination:" + destination + "\n\n" +
           message + "\n\0";
}

std::string StompProtocol::createSubscribeFrame(const std::string &destination, const std::string &id)
{
    return "SUBSCRIBE\n"
           "destination:" + destination + "\n"
           "id:" + id + "\n"
           "receipt:" + std::to_string(++receiptCounter) + "\n\n\0";
}

std::string StompProtocol::createUnsubscribeFrame(const std::string &id)
{
    for (auto it = subscriptions.begin(); it != subscriptions.end(); ++it)
    {
        if (it->second == id)
        {
            subscriptions.erase(it);
            break;
        }
    }
    return "UNSUBSCRIBE\n"
           "id:" + id + "\n\n\0";
}

std::string StompProtocol::createDisconnectFrame()
{
    return "DISCONNECT\n"
           "receipt:77\n\n\0";
}

void StompProtocol::processServerMessage(const std::string &message)
{
    if (message.find("CONNECTED") != std::string::npos)
    {
        std::cout << "Connected successfully to the server!" << std::endl;
    }
    else if (message.find("MESSAGE") != std::string::npos)
    {
        // diagnosed msg from channel
        std::string body = message.substr(message.find("\n\n") + 2);//change the +2
        std::cout << "New message received: " << body << std::endl;
    }
    else if (message.find("RECEIPT") != std::string::npos)
    {
        std::cout << "Action acknowledged by the server." << std::endl;
    }
    else if (message.find("ERROR") != std::string::npos)
    {
        std::cerr << "Error received from server: " << message << std::endl;
    }
    else
    {
        std::cerr << "Unknown message received: " << message << std::endl;
    }
}