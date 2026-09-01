//------------------------------------------------------------------------------
// File that defines the codification of the messages exchanged between the client and the server
//------------------------------------------------------------------------------
#ifndef __MESSAGES_HPP__
#define __MESSAGES_HPP__
#include "database_management.hpp"


class Message
{
private : 
    ID Message_Id; // message id
    Database_Manager& Database; // reference to the database manager for queries

public:
    enum Sender {SERVER_MESSAGE,CLIENT_MESSAGE};
    enum Type {AUTHENTIFICATION_REQUEST, AUTHENTIFICATION_SUCCESS, AUTHENTIFICATION_FAILURE_INPUT, AUTHENTIFICATION_FAILURE_USERNAME, AUTHENTIFICATION_FAILURE_PASSWORD,
                CLIENT_CONNECTED, CLIENT_DISCONNECTED, SERVER_SHUTDOWN, SERVER_RESTART, ACCUMULATING_ORDER, WAITING_ORDER, TRANSACTION,
                PRE_OPEN_PHASE, OPEN_PHASE, CONTINUOUS_TRADING_PHASE, PRE_CLOSE_PHASE, CLOSE_PHASE,
                DISPLAY_PORTFOLIO, DISPLAY_PENDING_ORDERS, DISPLAY_COMPLETED_ORDERS, DISPLAY_MARKET, DISPLAY_ACTION,
                EXIT, DEPOSIT, WITHDRAW, ORDER, ERROR}; 

    // constructor
    Message(const ID& message_id, Database_Manager& database);

    // setters
    void log_message(const ID& client_id, const Sender& message_sender, const Type& message_type, const std::string& content, const Time& time);

    // display the message
    void display_message() const;
};


#endif // __MESSAGES_HPP__