//=======================================================================
// File containing the definition of what a market is
//=======================================================================
#ifndef MARKET_HPP
#define MARKET_HPP
#include "database_management.hpp"


#include "client.hpp"
#include "messages.hpp"

// a row describing an order that is waiting for its trigger condition (LIMIT/STOP/LIMIT_STOP) or its expiration date, 
// Used by the server's trigger/expiration watcher thread to decide which orders should enter the live book (see server.cpp)
struct Waiting_Order_Row
{
    ID order_id;
    ID order_time_date;
    ID order_time_daily;
    ID client_id;
    Order_Type order_type;
    int quantity;
    ID action_id;
    Order_Trigger trigger_type;
    double price;
    double trigger_price_lower;
    double trigger_price_upper;
    ID expiration_time_date;
    ID expiration_time_daily;
};


class Market
{
private:
    // map where the key is the action id and the value is a vector of orders related to this action
    std::unordered_map<ID, std::vector<std::unique_ptr<Order>>> Buy_Orders; // buy orders for each action (refered by the action id)
    std::unordered_map<ID, std::vector<std::unique_ptr<Order>>> Sell_Orders; // sell orders for each action (refered by the action id)
    double Exchange_Price; // price of the transaction
    Database_Manager& Database; // reference to the database manager for queries (actions and clients)
    // callback used to push a real-time notification to a client concerned by an event (e.g. a transaction) that happened in some other client's thread:
    // Market stays network-agnostic, server.cpp is the one that supplies a callback that actually knows how to reach the client's socket
    std::function<void(const ID&, const std::string&)> Notify_Client;

    // price-time priority comparators: Buy_Orders[action_id] / Sell_Orders[action_id] are kept sorted by these at all times 
    static bool buy_order_priority(const std::unique_ptr<Order>& a, const std::unique_ptr<Order>& b);
    static bool sell_order_priority(const std::unique_ptr<Order>& a, const std::unique_ptr<Order>& b);

public:
    // constructor
    Market(Database_Manager& database); // simple init
    Market(const Market&) = delete; // delete copy constructor
    Market& operator=(const Market&) = delete; // delete copy assignment operator
    Market(Market&& other) noexcept; // implement move constructor
    Market& operator=(Market&& other) noexcept; // implement move assignment operator

    // getters
    Database_Manager& get_database() const;

    // real-time client notifications
    void set_notification_callback(std::function<void(const ID&, const std::string&)> notify_callback); // registers the function server.cpp uses to push a message to a client's socket

    // clients handling
    void deposit(const ID& client_id, const double& amount); // deposit funds into the account of a client
    void withdraw(const ID& client_id, const double& amount); // withdraw funds from the account of a client
    bool can_afford(const ID& client_id, const int& quantity, const double& price, const ID& action_id) const; // returns True if the amount can be withdrawn from the client balance
    bool has_shares(const ID& client_id, const ID& action_id, const int& quantity) const; // returns True if the action can be removed from the portfolio of the client
    bool client_exists(const ID& client_id) const; // check if a client exists
    bool client_name_exists(const std::string& client_name) const; // check if a client exists with the given name
    ID client_id_if_name_and_password_registered(const std::string& client_name, const std::string& client_password); // check if a client is registered with the given name and password and return its ID
    void add_client(const ID& client_id, const std::string& client_name, const std::string& client_password, const double& balance, std::unordered_map<ID, int> portfolio); // add a client to the market with its balance and portfolio (action_id and quantity)
    void remove_client(const ID& client_id); // remove a client from the market
    ID get_client_id_from_name(const std::string& client_name) const; // get the client id from a client name
    void update_client_portfolio(const ID& client_id, const Order_Type& order_type, const ID& action_id, const int& quantity, const double& price, const ID& daily_time, const ID& date_time); // update the portfolio of a client with a new action
    void add_order_to_client_completed_orders(const ID& client_id, const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily); // add an order to the completed orders of a client
    void add_order_to_client_pending_orders(const ID& client_id, const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily); // add an order to the pending orders of a client
    void remove_order_from_client_pending_orders(const ID& client_id, const ID& order_id); // remove an order from the pending orders of a client
    void add_order_to_client_waiting_orders(const ID& client_id, const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily); // add an order that has not reached its trigger condition yet (LIMIT/STOP/LIMIT_STOP), so it is not part of the live book
    void remove_order_from_client_waiting_orders(const ID& client_id, const ID& order_id); // remove a waiting order (it either triggered and moved to the live book, or it expired)
    std::vector<Waiting_Order_Row> get_waiting_orders() const; // get every order (of every client) that is currently waiting for its trigger condition or its expiration, used by the trigger/expiration watcher thread

    // actions handling
    bool action_exists(const ID& action_id) const; // check if an action exists
    void add_action(const ID& action_id, const std::string& name, const int& quantity, const double& price, const ID& daily_time, const ID& date_time); // add an action to the market
    void remove_action(const ID& action_id); // remove an action from the market
    double get_market_value() const; // get the market value (sum of the values of all the actions)
    double get_action_current_price(const ID& action_id) const; // get the current (last traded) price of an action, 0.0 if it has no price history

    // market functionment
    double resolve_execution_price(const ID& action_id, const Order_Trigger& buyer_trigger_type, const Order_Trigger& seller_trigger_type, const double& buyer_price, const double& seller_price) const; // resolve the real settlement price of a match, never using a MARKET order's placeholder priority price
    void accumulate_order(const ID& client_id, const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily); // accumulate an order to the market and sort the orders by priority (add the order to the pending orders for the client)    
    void deaccumulate_order(const ID& client_id, const ID& order_id,  const Order_Type& order_type, const ID& action_id); // remove an order from the pending orders of the client (if it exists) and remove it from the market orders by making again the market sorting
    void process_fixing(); // process the fixing of the price to order the transactions by priority
    void process_continuous_trading(const ID& action_id); // process continuous trading for one action

    // string representation methods
    std::string get_orders_info() const; // get the orders info as a string : order_time_date order_time_daily client_name order_type quantity action_name trigger_type price trigger_price_lower trigger_price_upper expiration_time_date expiration_time_daily,... (BUY then SELL orders)
    std::string get_actions_info() const; // get the actions info as a string : action_name quantity last_price time,...
    std::string get_market_info() const; // get the market info as a string : market_value;order_time_date order_time_daily client_name order_type quantity action_name trigger_type price trigger_price_lower trigger_price_upper expiration_time_date expiration_time_daily,... (BUY then SELL orders);action_name quantity last_price time,...
};


#endif // MARKET_HPP