//=======================================================================
// File that contains the definition of what a client is
//=======================================================================
#ifndef CLIENT_HPP
#define CLIENT_HPP
#include "database_management.hpp"


#include "order.hpp"
#include "action.hpp"


class Client
{
private:
    ID Id; // define the client id
    Database_Manager& Database; // reference to the database manager for queries
    
public:
    // constructors
    Client(const ID& id, Database_Manager& database); // simple init

    // getters
    ID get_id() const;
    double get_balance() const;

    bool is_action_in_portfolio(const ID& action_id) const; // check if an action is in the portfolio

    // balance management:
    void deposit(const double& amount); // deposit funds into the account
    void withdraw(const double& amount); // withdraw funds from the account
    bool can_afford(const int& quantity, const double& price, const ID& action_id) const; // returns True if the amount can be withdrawn

    // completed orders management:
    void add_completed_order(const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily); // add an order to the client's list of completed orders

    // pending orders management:
    void add_pending_order(const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily); // add an order to the client's list of pending orders
    void remove_pending_order(const ID& order_id); // remove a pending order by order id 

    // waiting orders management (LIMIT/STOP/LIMIT_STOP orders that have not yet reached their trigger condition, so they are not part of the live order book yet):
    void add_waiting_order(const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily); // add an order to the client's list of orders waiting for their trigger condition
    void remove_waiting_order(const ID& order_id); // remove a waiting order by order id (either because it triggered and entered the book, or because it expired)

    // portfolio management: 
    void add_action(const ID& action_id, const int& quantity, const double& price, const ID& daily_time, const ID& date_time); // add a quantity for a specific action and update its price if necessary
    void remove_action(const ID& action_id, const int& quantity, const double& price, const ID& daily_time, const ID& date_time); // remove a quantity for a specific action and update its price if necessary
    bool has_shares(const ID& action_id, const int& quantity) const; // returns True if the action can be removed
    void update_portfolio(const Order_Type& order_type, const ID& action_id, const int& quantity, const double& price, const ID& daily_time, const ID& date_time); // update the portfolio with a new action (modify the client balance also)

    // strings representation methods 
    std::string get_completed_orders_info() const; // get the completed orders info as a string : order_time_date order_time_daily client_name order_type quantity action_name trigger_type price trigger_price_lower trigger_price_upper expiration_time_date expiration_time_daily,...
    std::string get_pending_orders_info() const; // get the pending orders info as a string : order_time_date order_time_daily client_name order_type quantity action_name trigger_type price trigger_price_lower trigger_price_upper expiration_time_date expiration_time_daily,...
    std::string get_portfolio_info() const; // get the portfolio info as a string : value balance,action_name_1 quantity1 last_price1,action_name_2 quantity2 last_price2,...
};


#endif // CLIENT_HPP