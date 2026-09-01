//==========================================================================
// File containing the definition of what an order is
//==========================================================================
#ifndef ORDER_HPP
#define ORDER_HPP
#include "database_management.hpp"


enum class Order_Type
{
    BUY, 
    SELL
};
// converting a string to an Order_Type enum
Order_Type string_to_order_type(const std::string& order_type_str);
// converting an Order_Type enum to a string
std::string order_type_to_string(const Order_Type& order_type);


enum class Order_Trigger
{
    NO_TRIGGER, // no trigger, an error
    MARKET,
    LIMIT,
    STOP,
    LIMIT_STOP
};
// converting a string to an Order_Trigger enum
Order_Trigger string_to_trigger(const std::string& trigger_type_str);
// converting an Order_Trigger enum to a string
std::string trigger_to_string(const Order_Trigger& trigger_type);


class Order
{
private:
    ID Order_Id; // order id
    Database_Manager& Database; // reference to the database manager for queries

public:
    // constructor
    Order(const ID& order_id, Database_Manager& database); // simple init
    std::unique_ptr<Order> clone() const; // clone method to create a copy of the current Order object (useful in the market part)

    // getters
    ID get_order_id() const;
    ID get_date_order_time() const;
    ID get_daily_order_time() const;
    int get_quantity() const;
    double get_price() const;

    // setters
    void set_quantity(const int& new_quantity);

    // string representation methods for market usage
    std::string get_order_info() const; // get the order info as a string : order_id order_time_date order_time_daily client_id quantity trigger_type price trigger_price_lower trigger_price_upper expiration_time_date expiration_time_daily
};


#endif // ORDER_HPP