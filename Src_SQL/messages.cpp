#include "messages.hpp"


// constructor
Message::Message(const ID& message_id, Database_Manager& database) : Message_Id(message_id), Database(database)
{

}


// setters
void Message::log_message(const ID& client_id, const Sender& message_sender, const Type& message_type, const std::string& content, const Time& time)
{
    std::string sender = "CLIENT_MESSAGE";
    if (message_sender == Sender::SERVER_MESSAGE){
        sender = "SERVER_MESSAGE";
    }
    std::string type;
    switch (message_type){
        case Type::AUTHENTIFICATION_REQUEST:
            type = "AUTHENTIFICATION_REQUEST";
            break;
        case Type::AUTHENTIFICATION_SUCCESS:
            type = "AUTHENTIFICATION_SUCCESS";
            break;
        case Type::AUTHENTIFICATION_FAILURE_INPUT:
            type = "AUTHENTIFICATION_FAILURE_INPUT";
            break;
        case Type::AUTHENTIFICATION_FAILURE_USERNAME:
            type = "AUTHENTIFICATION_FAILURE_USERNAME";
            break;
        case Type::AUTHENTIFICATION_FAILURE_PASSWORD:
            type = "AUTHENTIFICATION_FAILURE_PASSWORD";
            break;
        case Type::CLIENT_CONNECTED:
            type = "CLIENT_CONNECTED";
            break;
        case Type::CLIENT_DISCONNECTED:
            type = "CLIENT_DISCONNECTED";
            break;
        case Type::SERVER_SHUTDOWN:
            type = "SERVER_SHUTDOWN";
            break;
        case Type::SERVER_RESTART:
            type = "SERVER_RESTART";
            break;
        case ACCUMULATING_ORDER:
            type = "ACCUMULATING_ORDER";
            break;
        case WAITING_ORDER:
            type = "WAITING_ORDER";
            break;
        case TRANSACTION:
            type = "TRANSACTION";
            break;
        case PRE_OPEN_PHASE:
            type = "PRE_OPEN_PHASE";
            break;
        case OPEN_PHASE:
            type = "OPEN_PHASE";
            break;
        case CONTINUOUS_TRADING_PHASE:
            type = "CONTINUOUS_TRADING_PHASE";
            break;
        case PRE_CLOSE_PHASE:
            type = "PRE_CLOSE_PHASE";
            break;
        case CLOSE_PHASE:
            type = "CLOSE_PHASE";
            break;
        case Type::DISPLAY_PORTFOLIO:
            type = "DISPLAY_PORTFOLIO";
            break;
        case Type::DISPLAY_PENDING_ORDERS:
            type = "DISPLAY_PENDING_ORDERS";
            break;
        case Type::DISPLAY_COMPLETED_ORDERS:
            type = "DISPLAY_COMPLETED_ORDERS";
            break;
        case Type::DISPLAY_MARKET:
            type = "DISPLAY_MARKET";
            break;
        case Type::DISPLAY_ACTION:
            type = "DISPLAY_ACTION";
            break;
        case Type::EXIT:
            type = "EXIT";
            break;
        case Type::DEPOSIT:
            type = "DEPOSIT";
            break;
        case Type::WITHDRAW:
            type = "WITHDRAW";
            break;
        case Type::ORDER:
            type = "ORDER";
            break;
        default:
            type = "ERROR";
            break;
    }
    ID message_daily_time = get_daily_time(time);
    ID message_date_time = get_date_time(time);
    std::ostringstream query;
    query << "INSERT INTO messages (message_id, client_id, message_sender, message_type, content, daily_time, date_time) VALUES (" << Message_Id << ", " << client_id << ", '" << sender << "', '" << type << "', '" << content << "', " << message_daily_time << ", " << message_date_time << ")";
    Database.execute_SQL(query.str());
}


// display the message
void Message::display_message() const
{   
    std::ostringstream query;
    query << "SELECT client_id, message_sender, message_type, content, daily_time, date_time FROM messages WHERE message_id = " << Message_Id;
    std::vector<std::vector<std::string>> message_info = Database.execute_SQL_query_vec_strings(query.str());
    if (message_info.empty()){
        std::cerr << "Error: Message not found.\n";
        return;
    }
    std::cout << "Message ID: " << Message_Id << ", Client ID: " << message_info[0][0] << ", Sender: " << message_info[0][1] << ", Type: " << message_info[0][2] << ", Content: " << message_info[0][3] << ",Time: " << two_times_to_string(std::stoll(message_info[0][5]), std::stoll(message_info[0][4])) << "\n";
}

