#pragma once

#include "../include/ConnectionHandler.h"
#include <string>
#include <map>
#include <vector>
#include "../include/ConnectionHandler.h"

// TODO: implement the STOMP protocol
class StompProtocol
{
private:
    ConnectionHandler &connectionHandler;   //ref to connection Handler
    std::map<std::string, std::string> subscriptions; //Save subscriptions by channels
    int receiptCounter; // Unique counter for receipts
public:
    StompProtocol(ConnectionHandler &handler);

    std::string createConnectFrame(const std::string &host, const std::string &username, const std::string &password);
    std::string createSendFrame(const std::string &destination, const std::string &message);
    std::string createSubscribeFrame(const std::string &destination, const std::string &id);
    std::string createUnsubscribeFrame(const std::string &id);
    std::string createDisconnectFrame (); //Reciept

    void processServerMessage(const std::string &message);

};
