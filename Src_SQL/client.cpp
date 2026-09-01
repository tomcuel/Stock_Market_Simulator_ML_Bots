#include "client.hpp"


// constructor
// simple init
Client::Client(const ID& id, Database_Manager& database) : Id(id), Database(database)
{

}


// getters
ID Client::get_id() const
{
    return Id;
}

double Client::get_balance() const
{   
    std::string query = fmt::format(
        "SELECT balance FROM clients WHERE client_id = {}",
        get_id()
    );
    return Database.execute_SQL_query_double(query);
}


// check if an action is in the portfolio
bool Client::is_action_in_portfolio(const ID& action_id) const
{   
    std::string query = fmt::format(
        "SELECT action_id FROM client_portfolio WHERE client_id = {} AND action_id = {}",
        get_id(), action_id
    );
    return Database.execute_SQL_query_ID(query) == action_id;
}


// balance management:
// deposit funds into the account
void Client::deposit(const double& amount)
{
    if (amount > 0){
        std::string query = fmt::format(
            "UPDATE clients SET balance = balance + {} WHERE client_id = {}",
            amount, 
            get_id()
        );
        Database.execute_SQL(query);
    }
}

// withdraw funds from the account 
void Client::withdraw(const double& amount)
{   
    // we already make sure that the amount is positive in can_afford, so we don't check it here
    std::string query = fmt::format(
        "UPDATE clients SET balance = balance - {} WHERE client_id = {}",
        amount, 
        get_id()
    );
    Database.execute_SQL(query);
}

// returns True if the amount can be withdrawn
bool Client::can_afford(const int& quantity, const double& price, const ID& action_id) const
{   
    double amount = 0.0;
    if (price == max_number && action_id != -1){ // market order are never executed at max_number, it's for the order sorting
        std::string price_query = fmt::format(
            "SELECT price FROM prices WHERE action_id = {} ORDER BY date_time DESC, daily_time DESC LIMIT 1",
            action_id
        );
        double current_price = Database.execute_SQL_query_double(price_query);
        if (current_price <= 0.0){ // no valid market price available
            return false;
        }
        // reserving a bit more than the current price to avoid the order being rejected if the price goes up a bit before the order is executed
        amount = quantity * current_price * safety_percentage;
    }
    else {
        amount = quantity * price;
    }
    if (amount < 0){ // invalid amount, should not happen
        return false;
    }

    // Calculate the money already reserved by pending and waiting orders
    // For MARKET orders: use the latest known market price, NOT max_number
    // For other orders: use the order's specified price
    const std::string query = fmt::format(
        R"(SELECT
            c.balance -
            COALESCE(
                (
                    SELECT SUM(
                        o.quantity *
                        CASE
                            WHEN o.trigger_type = 'MARKET' THEN
                                COALESCE(
                                    (
                                        SELECT p.price
                                        FROM prices p
                                        WHERE p.action_id = o.action_id
                                        ORDER BY
                                            p.date_time DESC,
                                            p.daily_time DESC
                                        LIMIT 1
                                    ),
                                    0
                                ) * {}
                            ELSE
                                o.price
                        END
                    )
                    FROM orders o
                    WHERE o.client_id = c.client_id AND (o.order_status = 'PENDING' OR o.order_status = 'WAITING')
                ),
                0
            )
        FROM clients c WHERE c.client_id = {})",
        safety_percentage,
        get_id()
    );
    const double available_balance = Database.execute_SQL_query_double(query);
    return amount <= available_balance;
}


// completed orders management:
// add an order to the client's list of orders
void Client::add_completed_order(const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily)
{
    std::string order_type_string = order_type_to_string(order_type);
    std::string trigger_type_string = trigger_to_string(trigger_type);
    std::string order_status_string = "COMPLETED";
    std::string query = fmt::format(
        "INSERT INTO orders (order_id, order_status, order_time_date, order_time_daily, client_id, order_type, quantity, action_id, trigger_type, price, trigger_price_lower, trigger_price_upper, expiration_time_date, expiration_time_daily) VALUES ({}, '{}', {}, {}, {}, '{}', {}, {}, '{}', {}, {}, {}, {}, {})",
        order_id,
        order_status_string,
        order_time_date,
        order_time_daily,
        get_id(),
        order_type_string,
        quantity,
        action_id,
        trigger_type_string,
        price,
        trigger_price_lower,
        trigger_price_upper,
        expiration_time_date,
        expiration_time_daily
    );
    Database.execute_SQL(query);
}


// pending orders management:
// add an order to the client's list of pending orders
void Client::add_pending_order(const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily)
{
    std::string order_type_string = order_type_to_string(order_type);
    std::string trigger_type_string = trigger_to_string(trigger_type);
    std::string order_status_string = "PENDING";
    std::string query = fmt::format(
        "INSERT INTO orders (order_id, order_status, order_time_date, order_time_daily, client_id, order_type, quantity, action_id, trigger_type, price, trigger_price_lower, trigger_price_upper, expiration_time_date, expiration_time_daily) VALUES ({}, '{}', {}, {}, {}, '{}', {}, {}, '{}', {}, {}, {}, {}, {})",
        order_id,
        order_status_string,
        order_time_date,
        order_time_daily,
        get_id(),
        order_type_string,
        quantity,
        action_id,
        trigger_type_string,
        price,
        trigger_price_lower,
        trigger_price_upper,
        expiration_time_date,
        expiration_time_daily
    );
    Database.execute_SQL(query);
}

// remove a pending order by order id
void Client::remove_pending_order(const ID& order_id)
{   
    std::string query = fmt::format(
        "DELETE FROM orders WHERE order_id = {} AND order_status = 'PENDING' AND client_id = {}",
        order_id,
        get_id()
    );
    Database.execute_SQL(query);
}


// waiting orders management:
// add an order to the client's list of orders waiting for their trigger condition (LIMIT, STOP or LIMIT_STOP orders
// that have not yet reached the price band the client asked for, so they must not be matched against the book yet)
void Client::add_waiting_order(const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily)
{
    std::string order_type_string = order_type_to_string(order_type);
    std::string trigger_type_string = trigger_to_string(trigger_type);
    std::string order_status_string = "WAITING";
    std::string query = fmt::format(
        "INSERT INTO orders (order_id, order_status, order_time_date, order_time_daily, client_id, order_type, quantity, action_id, trigger_type, price, trigger_price_lower, trigger_price_upper, expiration_time_date, expiration_time_daily) VALUES ({}, '{}', {}, {}, {}, '{}', {}, {}, '{}', {}, {}, {}, {}, {})",
        order_id,
        order_status_string,
        order_time_date,
        order_time_daily,
        get_id(),
        order_type_string,
        quantity,
        action_id,
        trigger_type_string,
        price,
        trigger_price_lower,
        trigger_price_upper,
        expiration_time_date,
        expiration_time_daily
    );
    Database.execute_SQL(query);
}

// remove a waiting order by order id
void Client::remove_waiting_order(const ID& order_id)
{
    std::string query = fmt::format(
        "DELETE FROM orders WHERE order_id = {} AND order_status = 'WAITING' AND client_id = {}",
        order_id,
        get_id()
    );
    Database.execute_SQL(query);
}


// Portfolio management:
// add a quantity for a specific action and update its price if necessary
void Client::add_action(const ID& action_id, const int& quantity, const double& price, const ID& daily_time, const ID& date_time)
{
    // if the action is already in the portfolio, we add the quantity
    if (is_action_in_portfolio(action_id)){
        std::string query = fmt::format(
            "UPDATE client_portfolio SET quantity = quantity + {} WHERE client_id = {} AND action_id = {}",
            quantity, 
            get_id(), 
            action_id
        );
        Database.execute_SQL(query);
    }
    // otherwise, we add the action to the portfolio
    else{
        std::string query = fmt::format(
            "INSERT INTO client_portfolio (client_id, action_id, quantity) VALUES ({}, {}, {})",
            get_id(), 
            action_id, 
            quantity
        );
        Database.execute_SQL(query);
    }

    // check if the price and time already exist in the prices table
    std::string check_query = fmt::format(
        "SELECT 1 FROM prices WHERE action_id = {} AND price = {} AND daily_time = {} AND date_time = {} LIMIT 1",
        action_id, 
        price, 
        daily_time, 
        date_time
    );
    std::vector<std::vector<std::string>> existing_price = Database.execute_SQL_query_vec_strings(check_query);
    // if no matching price-time exists, insert the new price-time
    if (existing_price.empty()){
        std::string query = fmt::format(
            "INSERT INTO prices (action_id, price, daily_time, date_time) VALUES ({}, {}, {}, {})",
            action_id, 
            price, 
            daily_time, 
            date_time
        );
        Database.execute_SQL(query);
    }
}

// remove a quantity for a specific action and update its price if necessary
void Client::remove_action(const ID& action_id, const int& quantity, const double& price, const ID& daily_time, const ID& date_time)
{
    // if the action is in the portfolio, we remove the quantity
    if (is_action_in_portfolio(action_id)){
        std::string query = fmt::format(
            "UPDATE client_portfolio SET quantity = MAX(quantity - {}, 0) WHERE client_id = {} AND action_id = {}",
            quantity, 
            get_id(), 
            action_id
        );
        Database.execute_SQL(query);
    }

    // check if the price and time already exist in the prices table
    std::string check_query = fmt::format(
        "SELECT 1 FROM prices WHERE action_id = {} AND price = {} AND daily_time = {} AND date_time = {} LIMIT 1",
        action_id, 
        price, 
        daily_time, 
        date_time
    );
    std::vector<std::vector<std::string>> existing_price = Database.execute_SQL_query_vec_strings(check_query);
    // if no matching price-time exists, insert the new price-time
    if (existing_price.empty()){
        std::string query = fmt::format(
            "INSERT INTO prices (action_id, price, daily_time, date_time) VALUES ({}, {}, {}, {})",
            action_id, 
            price, 
            daily_time, 
            date_time
        );
        Database.execute_SQL(query);
    }
}

// returns True if the action can be removed
bool Client::has_shares(const ID& action_id, const int& quantity) const
{
    std::string query = fmt::format(
        "SELECT quantity FROM client_portfolio WHERE client_id = {} AND action_id = {}",
        get_id(), 
        action_id
    );
    return quantity > 0 && quantity <= Database.execute_SQL_query_int(query);
}

// update the portfolio with a new action (modify the client balance also)
void Client::update_portfolio(const Order_Type& order_type, const ID& action_id, const int& quantity, const double& price, const ID& daily_time, const ID& date_time)
{
    if (order_type == Order_Type::BUY){
        if (can_afford(quantity, price, action_id)){
            withdraw(price * quantity);
            add_action(action_id, quantity, price, daily_time, date_time);
        }
        else {
            std::cerr << "Error: Insufficient balance for buying.\n";
        }
    }
    else if (order_type == Order_Type::SELL){
        if (has_shares(action_id, quantity)){
            deposit(price * quantity);
            remove_action(action_id, quantity, price, daily_time, date_time);
        }
        else {
            std::cerr << "Error: Failed to sell action.\n";
        }
    }
}


// string representation methods
// get the completed orders info as a string : order_time_date order_time_daily client_name order_type quantity action_name trigger_type price trigger_price_lower trigger_price_upper expiration_time_date expiration_time_daily,...
std::string Client::get_completed_orders_info() const
{   
    std::string query = fmt::format(
        R"(SELECT o.order_time_date, o.order_time_daily, c.name, o.order_type, o.quantity, a.name, o.trigger_type, o.price, o.trigger_price_lower, o.trigger_price_upper, o.expiration_time_date, o.expiration_time_daily
          FROM orders o JOIN actions a ON o.action_id = a.action_id JOIN clients c ON o.client_id = c.client_id
          WHERE o.client_id = {} AND o.order_status = 'COMPLETED')",
        get_id()
    );
    std::vector<std::vector<std::string>> completed_orders_info = Database.execute_SQL_query_vec_strings(query);
    
    // check if we have enough data before accessing elements
    if (completed_orders_info.empty()){
        return ""; // return empty if data is missing
    }
    
    std::string result;
    for (const auto& order : completed_orders_info){
        if (order.size() >= 12){
            result += fmt::format(
                "{} {} {} {} {} {} {} {} {} {},",
                two_times_to_string(std::stoll(order[0]), std::stoll(order[1])),
                order[2],
                order[3],
                order[4],
                order[5],
                order[6],
                order[7],
                order[8],
                order[9],
                two_times_to_string(std::stoll(order[10]), std::stoll(order[11]))
            );
        }
    }
    if (!result.empty()){
        result.pop_back(); // remove trailing comma
    }
    return result;
}

// get the pending orders info as a string : order_time_date order_time_daily client_name order_type quantity action_name trigger_type price trigger_price_lower trigger_price_upper expiration_time_date expiration_time_daily,...
std::string Client::get_pending_orders_info() const
{   
    std::string query = fmt::format(
        R"(SELECT o.order_time_date, o.order_time_daily, c.name, o.order_type, o.quantity, a.name, o.trigger_type, o.price, o.trigger_price_lower, o.trigger_price_upper, o.expiration_time_date, o.expiration_time_daily
          FROM orders o JOIN actions a ON o.action_id = a.action_id JOIN clients c ON o.client_id = c.client_id
          WHERE o.client_id = {} AND o.order_status = 'PENDING')",
        get_id()
    );
    std::vector<std::vector<std::string>> completed_orders_info = Database.execute_SQL_query_vec_strings(query);
    
    // check if we have enough data before accessing elements
    if (completed_orders_info.empty()){
        return ""; // return empty if data is missing
    }
    
    std::string result;
    for (const auto& order : completed_orders_info){
        if (order.size() >= 12){
            result += fmt::format(
                "{} {} {} {} {} {} {} {} {} {},",
                two_times_to_string(std::stoll(order[0]), std::stoll(order[1])),
                order[2],
                order[3],
                order[4],
                order[5],
                order[6],
                order[7],
                order[8],
                order[9],
                two_times_to_string(std::stoll(order[10]), std::stoll(order[11]))
            );
        }
    }
    if (!result.empty()){
        result.pop_back(); // remove trailing comma
    }
    return result;
}

// get the portfolio info as a string : value balance,action_name_1 quantity1 last_price1,action_name_2 quantity2 last_price2,...
std::string Client::get_portfolio_info() const
{
    std::string query = fmt::format(
        R"(SELECT a.name, cp.quantity, p.price, p.date_time, p.daily_time
            FROM client_portfolio cp 
            JOIN actions a ON cp.action_id = a.action_id 
            LEFT JOIN prices p ON cp.action_id = p.action_id
            WHERE cp.client_id = {} 
            AND (p.date_time, p.daily_time) = (
                SELECT p2.date_time, p2.daily_time FROM prices p2 
                WHERE p2.action_id = cp.action_id 
                AND p2.date_time = (SELECT MAX(p3.date_time) FROM prices p3 WHERE p3.action_id = p2.action_id) 
                AND p2.daily_time = (SELECT MAX(p3.daily_time) FROM prices p3 WHERE p3.action_id = p2.action_id AND p3.date_time = p2.date_time)
            ) ORDER BY a.action_id ASC)",
        get_id()
    );
    std::vector<std::vector<std::string>> portfolio_info = Database.execute_SQL_query_vec_strings(query);
    
    // check if we have enough data before accessing elements
    if (portfolio_info.empty()){
        // return the balance if no data is found
        std::string result = fmt::format(
            "0.0 {},", 
            get_balance()
        );
        return result;
    }

    double portfolio_value = 0.0;
    std::string result = fmt::format(
        "{},", 
        get_balance()
    ); // add balance first and portfolio value will be added later
    // iterate over the portfolio info to calculate value and format the output
    for (const auto& row : portfolio_info){
        if (row.size() >= 5){
            int quantity = std::stoi(row[1]);
            double price = std::stod(row[2]);
            portfolio_value += quantity * price;
            result += fmt::format(
                "{} {} {} {},", 
                row[0], // action name
                quantity, 
                price, 
                two_times_to_string(std::stoll(row[4]), std::stoll(row[3]))
            );
        }
    }
    // remove the trailing comma
    if (!result.empty()){
        result.pop_back();
    }
    // add portfolio value at the start
    std::string full_result = fmt::format(
        "{} {}", 
        portfolio_value, 
        result
    );
    return full_result;
}

