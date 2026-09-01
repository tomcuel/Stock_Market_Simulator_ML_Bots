#include "order.hpp"


// converting a string to an Order_Type enum
Order_Type string_to_order_type(const std::string& order_type_str)
{
    static const std::unordered_map<std::string, Order_Type> order_type_map = {
        {"BUY", Order_Type::BUY},
        {"SELL", Order_Type::SELL}
    };

    auto it = order_type_map.find(order_type_str);
    return (it != order_type_map.end()) ? it->second : Order_Type::BUY; // default value, error case
}

// converting an Order_Type enum to a string
std::string order_type_to_string(const Order_Type& order_type)
{
    static const std::unordered_map<Order_Type, std::string> order_type_map = {
        {Order_Type::BUY, "BUY"},
        {Order_Type::SELL, "SELL"}
    };

    auto it = order_type_map.find(order_type);
    return (it != order_type_map.end()) ? it->second : "BUY"; // default value, error case
}


// converting a string to an Order_Trigger enum
Order_Trigger string_to_trigger(const std::string& trigger_type_str)
{
    static const std::unordered_map<std::string, Order_Trigger> trigger_map = {
        {"MARKET", Order_Trigger::MARKET},
        {"LIMIT", Order_Trigger::LIMIT},
        {"STOP", Order_Trigger::STOP},
        {"LIMIT_STOP", Order_Trigger::LIMIT_STOP}
    };

    auto it = trigger_map.find(trigger_type_str);
    return (it != trigger_map.end()) ? it->second : Order_Trigger::NO_TRIGGER; // default value, error case
}

// converting an Order_Trigger enum to a string
std::string trigger_to_string(const Order_Trigger& trigger_type)
{
    static const std::unordered_map<Order_Trigger, std::string> trigger_map = {
        {Order_Trigger::MARKET, "MARKET"},
        {Order_Trigger::LIMIT, "LIMIT"},
        {Order_Trigger::STOP, "STOP"},
        {Order_Trigger::LIMIT_STOP, "LIMIT_STOP"},
        {Order_Trigger::NO_TRIGGER, "NO_TRIGGER"} // error case
    };  

    auto it = trigger_map.find(trigger_type);
    return (it != trigger_map.end()) ? it->second : "NO_TRIGGER"; // default value, error case
}


// constructor
Order::Order(const ID& order_id, Database_Manager& database) : Order_Id(order_id), Database(database)
{
    
}

// clone method to create a copy of the current Order object (useful in the market part)
std::unique_ptr<Order> Order::clone() const
{
    return std::make_unique<Order>(*this); // assuming the copy constructor is properly defined
}


// getters
ID Order::get_order_id() const
{
    return Order_Id;
}

ID Order::get_date_order_time() const
{   
    std::string query = fmt::format(
        "SELECT order_time_date FROM orders WHERE order_id = {}",
        get_order_id()
    );
    return Database.execute_SQL_query_ID(query);
}

ID Order::get_daily_order_time() const
{   
    std::string query = fmt::format(
        "SELECT order_time_daily FROM orders WHERE order_id = {}",
        get_order_id()
    );
    return Database.execute_SQL_query_ID(query);
}

int Order::get_quantity() const
{   
    std::string query = fmt::format(
        "SELECT quantity FROM orders WHERE order_id = {}",
        get_order_id()
    );
    return Database.execute_SQL_query_int(query);
}

double Order::get_price() const
{   
    std::string query = fmt::format(
        "SELECT price FROM orders WHERE order_id = {}",
        get_order_id()
    );
    return Database.execute_SQL_query_double(query);
}


// setters
void Order::set_quantity(const int& new_quantity)
{   
    if (new_quantity < 0) {
        throw std::invalid_argument("Quantity cannot be negative");
    }
    std::string query = fmt::format(
        "UPDATE orders SET quantity = {} WHERE order_id = {}",
        new_quantity,
        get_order_id()
    );
    Database.execute_SQL(query);
}


// string representation methods for market usage
// get the order info as a string : order_id order_time_date order_time_daily client_id quantity trigger_type price trigger_price_lower trigger_price_upper expiration_time_date expiration_time_daily
std::string Order::get_order_info() const
{   
    std::string query = fmt::format(
        "SELECT order_id, order_time_date, order_time_daily, client_id, quantity, trigger_type, price, trigger_price_lower, trigger_price_upper, expiration_time_date, expiration_time_daily FROM orders WHERE order_id = {}",
        get_order_id()
    );
    std::vector<std::vector<std::string>> order_info = Database.execute_SQL_query_vec_strings(query);
    
    // check if we have enough data before accessing elements
    if (order_info.empty() || order_info[0].size() < 11){
        return ""; // return empty if data is missing
    }
    
    std::string order_infos = fmt::format(
        "{} {} {} {} {} {} {} {} {} {} {}",
        order_info[0][0],
        order_info[0][1], 
        order_info[0][2],
        order_info[0][3],
        order_info[0][4],
        order_info[0][5],
        order_info[0][6],
        order_info[0][7],
        order_info[0][8],
        order_info[0][9], 
        order_info[0][10]
    );
    return order_infos;
}

