#include "market.hpp"


// constructor
Market::Market(Database_Manager& database) : Exchange_Price(0.0), Database(database)
{

}

// implement a move constructor
Market::Market(Market&& other) noexcept : Buy_Orders(std::move(other.Buy_Orders)), Sell_Orders(std::move(other.Sell_Orders)), Exchange_Price(other.Exchange_Price), Database(other.Database), Notify_Client(std::move(other.Notify_Client))
{

}

// implement move assignment operator
Market& Market::operator=(Market&& other) noexcept
{
    if (this != &other){
        Buy_Orders = std::move(other.Buy_Orders);
        Sell_Orders = std::move(other.Sell_Orders);
        Exchange_Price = other.Exchange_Price;
        Notify_Client = std::move(other.Notify_Client);
        // Database reference remains unchanged
    }
    return *this;
}


// getters
Database_Manager& Market::get_database() const
{
    return Database;
}


// real-time client notifications
void Market::set_notification_callback(std::function<void(const ID&, const std::string&)> notify_callback)
{
    Notify_Client = std::move(notify_callback);
}


// clients handling
// deposit funds into the account of a client
void Market::deposit(const ID& client_id, const double& amount)
{
    Client client(client_id, Database);
    client.deposit(amount);
}

// withdraw funds from the account of a client
void Market::withdraw(const ID& client_id, const double& amount)
{
    Client client(client_id, Database);
    client.withdraw(amount);
}

// returns True if the amount can be withdrawn from the client balance
bool Market::can_afford(const ID& client_id, const int& quantity, const double& price, const ID& action_id) const
{
    Client client(client_id, Database);
    return client.can_afford(quantity, price, action_id);
}

// returns True if the action can be removed from the portfolio of the client
bool Market::has_shares(const ID& client_id, const ID& action_id, const int& quantity) const
{
    Client client(client_id, Database);
    return client.has_shares(action_id, quantity);
}

// check if a client exists
bool Market::client_exists(const ID& client_id) const
{
    std::string query = fmt::format(
        "SELECT client_id FROM clients WHERE client_id = {}",
        client_id
    );
    return Database.execute_SQL_query_ID(query) == client_id;
}

// check if a client exists with the given name
bool Market::client_name_exists(const std::string& client_name) const
{
    std::string query = fmt::format(
        "SELECT client_id FROM clients WHERE name = '{}'",
        client_name
    );
    return Database.execute_SQL_query_ID(query) != -1;
}

// check if a client is registered with the given name and password and return its ID
ID Market::client_id_if_name_and_password_registered(const std::string& client_name, const std::string& client_password)
{
    ID client_id = -1;  // default value in case no client is found
    // convert the client password to the appropriate encrypted format (binary)
    std::vector<unsigned char> encrypted_password(client_password.begin(), client_password.end());

    // prepare the SQL query to search for the client by name and encrypted password
    std::string query = "SELECT client_id FROM clients WHERE name = ? AND encrypted_password = ?";

    // raw sqlite3 API calls bypass Database's own wrapper methods (which lock internally), so this has to take Database's mutex itself 
    // every client authentication goes through here concurrently
    std::lock_guard<std::mutex> lock(Database.get_mutex());

    sqlite3_stmt* stmt;
    // prepare the SQL query
    if (sqlite3_prepare_v2(Database.get_database(), query.c_str(), -1, &stmt, nullptr) != SQLITE_OK){
        std::cerr << "Error preparing SQL: " << sqlite3_errmsg(Database.get_database()) << std::endl;
        return client_id;
    }

    // bind the client_name to the first placeholder
    sqlite3_bind_text(stmt, 1, client_name.c_str(), -1, SQLITE_STATIC);
    // bind the encrypted_password (as BLOB) to the second placeholder
    sqlite3_bind_blob(stmt, 2, encrypted_password.data(), encrypted_password.size(), SQLITE_STATIC);

    // execute the query
    if (sqlite3_step(stmt) == SQLITE_ROW){
        // fetch the client ID from the result
        client_id = sqlite3_column_int(stmt, 0); // assuming the first column is the client_id
    }
    // finalize the statement
    sqlite3_finalize(stmt);
    return client_id;
}

// add a client to the market with its balance and portfolio (action_id and quantity)
void Market::add_client(const ID& client_id, const std::string& client_name, const std::string& client_password, const double& balance, std::unordered_map<ID, int> portfolio)
{
    // convert the std::string password to std::vector<unsigned char> for storage as BLOB
    std::vector<unsigned char> encrypted_password(client_password.begin(), client_password.end());
    
    // insert client into the "clients" table
    std::string query = "INSERT INTO clients (client_id, name, encrypted_password, balance) VALUES (?, ?, ?, ?)";
    {
        // raw sqlite3 API calls bypass Database's own wrapper methods (which lock internally), so this block has to take Database's mutex itself (see the same note in client_id_if_name_and_password_registered)
        // scoped tightly to just this block: execute_SQL() below (for the portfolio inserts) locks internally too, and std::mutex isn't recursive
        // holding this for the whole function would deadlock
        std::lock_guard<std::mutex> lock(Database.get_mutex());
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(Database.get_database(), query.c_str(), -1, &stmt, nullptr) == SQLITE_OK){
            // bind client_id (INTEGER)
            sqlite3_bind_int(stmt, 1, client_id);
            // bind client_name (TEXT)
            sqlite3_bind_text(stmt, 2, client_name.c_str(), -1, SQLITE_STATIC);
            // bind encrypted_password (BLOB)
            sqlite3_bind_blob(stmt, 3, encrypted_password.data(), encrypted_password.size(), SQLITE_STATIC);
            // bind balance (REAL)
            sqlite3_bind_double(stmt, 4, balance);
            // execute the insert statement
            if (sqlite3_step(stmt) != SQLITE_DONE){
                std::cerr << "Error inserting client into database: " << sqlite3_errmsg(Database.get_database()) << std::endl;
            }
            sqlite3_finalize(stmt);  // finalize the statement after execution
        }
        else {
            std::cerr << "Error preparing SQL insert statement for client: " << sqlite3_errmsg(Database.get_database()) << std::endl;
        }
    }

    for (const auto& [action_id, quantity] : portfolio){
        // the action will not already be in the client's portfolio since we are creating the client
        std::string query = fmt::format(
            "INSERT INTO client_portfolio (client_id, action_id, quantity) VALUES ({}, {}, {})",
            client_id,
            action_id,
            quantity
        );
        Database.execute_SQL(query);
    }
}

// remove a client from the market
void Market::remove_client(const ID& client_id)
{   
    std::string query = fmt::format(
        "DELETE FROM clients WHERE client_id = {}",
        client_id
    );
    Database.execute_SQL(query);
    query = fmt::format(
        "DELETE FROM client_portfolio WHERE client_id = {}",
        client_id
    );
    Database.execute_SQL(query);
    query = fmt::format(
        "DELETE FROM orders WHERE client_id = {}",
        client_id
    );
    Database.execute_SQL(query);
}

// get the client id from a client name
ID Market::get_client_id_from_name(const std::string& client_name) const 
{
    std::string query = fmt::format(
        "SELECT client_id FROM clients WHERE name = '{}'",
        client_name
    );
    return Database.execute_SQL_query_ID(query);
}

// update the portfolio of a client with a new action
void Market::update_client_portfolio(const ID& client_id, const Order_Type& order_type, const ID& action_id, const int& quantity, const double& price, const ID& daily_time, const ID& date_time)
{
    Client client(client_id, Database);
    client.update_portfolio(order_type, action_id, quantity, price, daily_time, date_time);
}

// add an order to the completed orders of a client
void Market::add_order_to_client_completed_orders(const ID& client_id, const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily)
{
    Client client(client_id, Database);
    client.add_completed_order(order_id, order_time_date, order_time_daily, order_type, quantity, action_id, trigger_type, price, trigger_price_lower, trigger_price_upper, expiration_time_date, expiration_time_daily);
}

// add an order to the pending orders of a client
void Market::add_order_to_client_pending_orders(const ID& client_id, const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily)
{
    Client client(client_id, Database);
    client.add_pending_order(order_id, order_time_date, order_time_daily, order_type, quantity, action_id, trigger_type, price, trigger_price_lower, trigger_price_upper, expiration_time_date, expiration_time_daily);
}

// remove an order from the pending orders of a client
void Market::remove_order_from_client_pending_orders(const ID& client_id, const ID& order_id)
{
    Client client(client_id, Database);
    client.remove_pending_order(order_id);
}

// add an order that has not reached its trigger condition yet to the waiting orders of a client
void Market::add_order_to_client_waiting_orders(const ID& client_id, const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily)
{
    Client client(client_id, Database);
    client.add_waiting_order(order_id, order_time_date, order_time_daily, order_type, quantity, action_id, trigger_type, price, trigger_price_lower, trigger_price_upper, expiration_time_date, expiration_time_daily);
}

// remove an order from the waiting orders of a client
void Market::remove_order_from_client_waiting_orders(const ID& client_id, const ID& order_id)
{
    Client client(client_id, Database);
    client.remove_waiting_order(order_id);
}

// get every order (of every client) that is currently waiting for its trigger condition or its expiration
std::vector<Waiting_Order_Row> Market::get_waiting_orders() const
{
    std::string query = R"(
        SELECT order_id, order_time_date, order_time_daily, client_id, order_type, quantity, action_id, trigger_type, price, trigger_price_lower, trigger_price_upper, expiration_time_date, expiration_time_daily
        FROM orders
        WHERE order_status = 'WAITING'
    )";
    std::vector<std::vector<std::string>> rows = Database.execute_SQL_query_vec_strings(query);

    std::vector<Waiting_Order_Row> waiting_orders;
    for (const auto& row : rows){
        if (row.size() < 13){
            continue; // malformed row, skip it defensively
        }
        Waiting_Order_Row waiting_order;
        waiting_order.order_id = std::stoll(row[0]);
        waiting_order.order_time_date = std::stoll(row[1]);
        waiting_order.order_time_daily = std::stoll(row[2]);
        waiting_order.client_id = std::stoll(row[3]);
        waiting_order.order_type = string_to_order_type(row[4]);
        waiting_order.quantity = std::stoi(row[5]);
        waiting_order.action_id = std::stoll(row[6]);
        waiting_order.trigger_type = string_to_trigger(row[7]);
        waiting_order.price = std::stod(row[8]);
        waiting_order.trigger_price_lower = std::stod(row[9]);
        waiting_order.trigger_price_upper = std::stod(row[10]);
        waiting_order.expiration_time_date = std::stoll(row[11]);
        waiting_order.expiration_time_daily = std::stoll(row[12]);
        waiting_orders.push_back(waiting_order);
    }
    return waiting_orders;
}


// actions handling
// check if an action exists
bool Market::action_exists(const ID& action_id) const
{
    std::string query = fmt::format(
        "SELECT action_id FROM actions WHERE action_id = {}",
        action_id
    );
    return Database.execute_SQL_query_ID(query) == action_id;
}

// add an action to the market
void Market::add_action(const ID& action_id, const std::string& name, const int& quantity, const double& price, const ID& daily_time, const ID& date_time)
{
    // if the action is already in the market, we add the quantity
    if (action_exists(action_id)){
        std::string query = fmt::format(
            "UPDATE actions SET quantity = quantity + {} WHERE action_id = {}",
            quantity,
            action_id
        );
        Database.execute_SQL(query);
    }
    // otherwise, we add the action to the market
    else {
        std::string query = fmt::format(
            "INSERT INTO actions (action_id, name, quantity) VALUES ({}, '{}', {})",
            action_id,
            name,
            quantity
        );
        Database.execute_SQL(query);
    }
    std::string query2 = fmt::format(
        "INSERT INTO prices (action_id, price, date_time, daily_time) VALUES ({}, {}, {}, {})",
        action_id,
        price,
        date_time,
        daily_time
    );
    Database.execute_SQL(query2);
}

// remove an action from the market
void Market::remove_action(const ID& action_id)
{
    std::string query = fmt::format(
        "DELETE FROM actions WHERE action_id = {}",
        action_id
    );
    Database.execute_SQL(query);
    query = fmt::format(
        "DELETE FROM prices WHERE action_id = {}",
        action_id
    );
    Database.execute_SQL(query);
    query = fmt::format(
        "DELETE FROM orders WHERE action_id = {}",
        action_id
    );
    Database.execute_SQL(query);
    query = fmt::format(
        "DELETE FROM client_portfolio WHERE action_id = {}",
        action_id
    );
    Database.execute_SQL(query);
}

// get the market value (sum of the values of all the actions)
double Market::get_market_value() const
{
    std::string query = R"(
        SELECT SUM(a.quantity * p.price) 
        FROM actions a 
        LEFT JOIN prices p ON a.action_id = p.action_id
        WHERE (p.date_time, p.daily_time) = (
            SELECT p2.date_time, p2.daily_time
            FROM prices p2
            WHERE p2.action_id = a.action_id
            AND p2.date_time = (
                SELECT MAX(date_time)
                FROM prices
                WHERE action_id = p2.action_id
            )
            AND p2.daily_time = (
                SELECT MAX(daily_time)
                FROM prices
                WHERE action_id = p2.action_id
                AND date_time = p2.date_time
            )
        )
    )";
    return Database.execute_SQL_query_double(query);
}

// get the current (last traded) price of an action, 0.0 if it has no price history
double Market::get_action_current_price(const ID& action_id) const
{
    Action action(action_id, Database);
    return action.get_current_price();
}


// market functionment
// resolve the price at which a matched trade must actually be settled
// a MARKET order's stored `price` field (max_number for BUY, 0.0 for SELL) is only a priority placeholder used to sort the book (it must never be used as the traded price)
// It returns the correct execution price given the trigger type:
//   - if only one side is a MARKET order, the trade executes at the other side's real limit price (the market order simply accepts whatever price the resting limit order is offering)
//   - if both sides are MARKET orders, there is no limit price to fall back on, so we use the last known traded/reference price for the action (falling back to the previous fixing price of this session if the action has no price history yet)
//   - if both sides are LIMIT orders, we keep the existing convention of executing at the price of the resting order that has been standing the longest (the earliest one in this action's priority queue), defaulting to the seller's price which matches the prior behavior
double Market::resolve_execution_price(const ID& action_id, const Order_Trigger& buyer_trigger_type, const Order_Trigger& seller_trigger_type, const double& buyer_price, const double& seller_price) const
{
    bool buyer_is_market = (buyer_trigger_type == Order_Trigger::MARKET);
    bool seller_is_market = (seller_trigger_type == Order_Trigger::MARKET);

    if (buyer_is_market && seller_is_market){
        double reference_price = get_action_current_price(action_id);
        // if the action has no price history yet, fall back to the last price fixed during this session
        return (reference_price > 0.0) ? reference_price : Exchange_Price;
    }
    if (seller_is_market){
        return buyer_price; // seller accepts any price, so the buyer's real limit price sets the trade
    }
    if (buyer_is_market){
        return seller_price; // buyer accepts any price, so the seller's real limit price sets the trade
    }
    // both are limit orders: keep the existing convention (seller's resting price)
    return seller_price;
}

// price-time priority comparators
bool Market::buy_order_priority(const std::unique_ptr<Order>& a, const std::unique_ptr<Order>& b)
{
    if (a->get_price() != b->get_price()){
        return a->get_price() > b->get_price();                     // higher price first
    }
    if (a->get_date_order_time() != b->get_date_order_time()){
        return a->get_date_order_time() < b->get_date_order_time(); // earlier date first
    }
    return a->get_daily_order_time() < b->get_daily_order_time();   // earlier intraday time
}

bool Market::sell_order_priority(const std::unique_ptr<Order>& a, const std::unique_ptr<Order>& b)
{
    if (a->get_price() != b->get_price()){
        return a->get_price() < b->get_price();                     // lower price first
    }
    if (a->get_date_order_time() != b->get_date_order_time()){
        return a->get_date_order_time() < b->get_date_order_time(); // earlier date first
    }
    return a->get_daily_order_time() < b->get_daily_order_time();   // earlier intraday time
}

// accumulate an order to the market and sort the orders by priority (add the order to the pending orders for the client)
void Market::accumulate_order(const ID& client_id, const ID& order_id, const ID& order_time_date, const ID& order_time_daily, const Order_Type& order_type, const int& quantity, const ID& action_id, const Order_Trigger& trigger_type, const double& price, const double& trigger_price_lower, const double& trigger_price_upper, const ID& expiration_time_date, const ID& expiration_time_daily)
{
    // check if the client exists
    if (!client_exists(client_id)){
        std::cerr << "Error: Client with ID " << client_id << " not found.\n";
        return;
    }

    // add the order to the pending orders of the client
    add_order_to_client_pending_orders(client_id, order_id, order_time_date, order_time_daily, order_type, quantity, action_id, trigger_type, price, trigger_price_lower, trigger_price_upper, expiration_time_date, expiration_time_daily);

    // create a shared pointer for the new order
    auto order = std::make_unique<Order>(order_id, Database);

    // accumulate the order to the market, inserted at its correct price-time priority position so Buy_Orders[action_id] / Sell_Orders[action_id] stay sorted without ever needing a full re-sort
    if (order_type == Order_Type::BUY){
        auto& orders = Buy_Orders[action_id];
        auto it = std::lower_bound(orders.begin(), orders.end(), order, buy_order_priority);
        orders.insert(it, std::move(order));
    }
    else if (order_type == Order_Type::SELL){
        auto& orders = Sell_Orders[action_id];
        auto it = std::lower_bound(orders.begin(), orders.end(), order, sell_order_priority);
        orders.insert(it, std::move(order));
    }
}

// remove an order from the pending orders of the client (if it exists) and remove it from the market orders by making again the market sorting
void Market::deaccumulate_order(const ID& client_id, const ID& order_id, const Order_Type& order_type, const ID& action_id)
{
    // check if the client exists
    if (!client_exists(client_id)){
        std::cerr << "Error: Client with ID " << client_id << " not found.\n";
        return;
    }

    // remove the order from the pending orders of the client
    remove_order_from_client_pending_orders(client_id, order_id);

    // create a shared pointer for the order to remove
    auto order = std::make_unique<Order>(order_id, Database);

     // Select the appropriate order list
    auto& orders = (order_type == Order_Type::BUY) ? Buy_Orders[action_id] : Sell_Orders[action_id];
    
    // find the matching order and erase it
    auto it = std::find_if(orders.begin(), orders.end(),
        [&](const std::unique_ptr<Order>& o){
            return o->get_order_id() == order_id;
        });

    if (it != orders.end()){
        orders.erase(it);
    } 
    else {
        std::cerr << "Warning: Order with ID " << order_id << " not found in market orders.\n";
    }

}

// process the fixing of the price to order the transactions by priority and update the client's portfolio
// Buy_Orders[action_id] and Sell_Orders[action_id] are always kept in price-time priority order (see accumulate_order() and the in-place replacement of partially-filled orders below)
// Buy prices only decrease and sell prices only increase as the two indices below advance, 
// a partial-fill leftoverkeeps being compared against the next best order on the other side within the very same inner while loop, until it is either fully filled or no more crossing is possible for that action 
// (at which point no other pair further down the book could cross either)
void Market::process_fixing()
{
    for (auto& [action_id, orders] : Buy_Orders){
        auto sell_it = Sell_Orders.find(action_id);
        if (sell_it == Sell_Orders.end() || orders.empty() || sell_it->second.empty()){
            continue; // nothing to do for this action: only one side (or neither) has resting orders
        }
        auto& sell_orders_for_action = sell_it->second;

        std::size_t buy_index = 0, sell_index = 0;
        while (buy_index < orders.size() && sell_index < sell_orders_for_action.size()){

            // getting some useful information
            auto& buy_order = orders[buy_index];
            auto& sell_order = sell_orders_for_action[sell_index];

            // buyer 
            std::string buyer_order_info = buy_order->get_order_info();
            std::istringstream buyer_stream(buyer_order_info);
            ID buyer_order_id, buyer_order_time_date, buyer_order_time_daily, buyer_client_id, buyer_expiration_time_date, buyer_expiration_time_daily;
            std::string buyer_trigger_type_str;
            int buyer_quantity;
            double buyer_price, buyer_trigger_price_lower, buyer_trigger_price_upper;
            // extract the values
            buyer_stream >> buyer_order_id 
                            >> buyer_order_time_date 
                            >> buyer_order_time_daily 
                            >> buyer_client_id 
                            >> buyer_quantity 
                            >> buyer_trigger_type_str 
                            >> buyer_price 
                            >> buyer_trigger_price_lower 
                            >> buyer_trigger_price_upper 
                            >> buyer_expiration_time_date 
                            >> buyer_expiration_time_daily;
            Order_Trigger buyer_trigger_type = string_to_trigger(buyer_trigger_type_str);

            // seller
            std::string seller_order_info = sell_order->get_order_info();
            std::istringstream seller_stream(seller_order_info);
            ID seller_order_id, seller_order_time_date, seller_order_time_daily, seller_client_id, seller_expiration_time_date, seller_expiration_time_daily;
            std::string seller_trigger_type_str;
            int seller_quantity;
            double seller_price, seller_trigger_price_lower, seller_trigger_price_upper;
            // extract the values
            seller_stream >> seller_order_id 
                            >> seller_order_time_date 
                            >> seller_order_time_daily 
                            >> seller_client_id 
                            >> seller_quantity 
                            >> seller_trigger_type_str 
                            >> seller_price 
                            >> seller_trigger_price_lower 
                            >> seller_trigger_price_upper 
                            >> seller_expiration_time_date 
                            >> seller_expiration_time_daily;
            Order_Trigger seller_trigger_type = string_to_trigger(seller_trigger_type_str);

            // check if clients exist
            if (!client_exists(buyer_client_id) || !client_exists(seller_client_id)){
                std::cerr << "Error: One of the clients does not exist in the market.\n";
                break;
            }

            // check if the price conditions are met: 
            // since buy orders are sorted best-price-first and sell orders are sorted best-price-first too, once this fails it will fail for every remaining pair as well (prices can only get worse from here)
            if (buyer_price < seller_price){
                break; // no possible transaction for this action
            }

            //  the quantities must be positive
            if (buyer_quantity <= 0 || seller_quantity <= 0){
                break; // no possible transaction for this action
            }

            // perform transaction between buyer and seller
            int transaction_quantity = std::min(buyer_quantity, seller_quantity);
            Exchange_Price = resolve_execution_price(action_id, buyer_trigger_type, seller_trigger_type, buyer_price, seller_price);
            Time exchange_time = get_current_time_ms();
            ID exchange_time_daily = get_daily_time(exchange_time);
            ID exchange_time_date = get_date_time(exchange_time);

            // update the client's portfolio
            update_client_portfolio(buyer_client_id, Order_Type::BUY, action_id, transaction_quantity, Exchange_Price, exchange_time_daily, exchange_time_date);
            update_client_portfolio(seller_client_id, Order_Type::SELL, action_id, transaction_quantity, Exchange_Price, exchange_time_daily, exchange_time_date);

            // log transaction details
            std::string transaction_details = fmt::format(
                "Transaction of {} actions {} at the price of {}$ between buyer {} and seller {} at time {}",
                transaction_quantity, 
                action_id, 
                Exchange_Price, 
                buyer_client_id, 
                seller_client_id, 
                time_to_string(exchange_time)
            );
            Message transaction_message(get_database().get_new_message_id(), get_database());
            transaction_message.log_message(
                0, 
                Message::Sender::SERVER_MESSAGE, 
                Message::Type::TRANSACTION, 
                transaction_details,
                exchange_time
            );
            // push the notification to whichever client is concerned, even if the match happened on another client's thread
            if (Notify_Client){
                Notify_Client(buyer_client_id, fmt::format(
                    "TRANSACTION: bought {} of action {} at {}$ at time {}",
                    transaction_quantity, action_id, Exchange_Price, time_to_string(exchange_time)
                ));
                Notify_Client(seller_client_id, fmt::format(
                    "TRANSACTION: sold {} of action {} at {}$ at time {}",
                    transaction_quantity, action_id, Exchange_Price, time_to_string(exchange_time)
                ));
            }

            // the filled portion always closes out both orders in the database, a fresh order id is opened below for whichever side has quantity left over, so its full lifecycle stays visible in that client's completed order history
            remove_order_from_client_pending_orders(buyer_client_id, buyer_order_id);
            remove_order_from_client_pending_orders(seller_client_id, seller_order_id);

            // add executed portion to completed orders
            if (transaction_quantity > 0){
                add_order_to_client_completed_orders(buyer_client_id, get_database().get_new_order_id(), exchange_time_date, exchange_time_daily, Order_Type::BUY, transaction_quantity, action_id, buyer_trigger_type, Exchange_Price, buyer_trigger_price_lower, buyer_trigger_price_upper, buyer_expiration_time_date, buyer_expiration_time_daily);
                add_order_to_client_completed_orders(seller_client_id, get_database().get_new_order_id(), exchange_time_date, exchange_time_daily, Order_Type::SELL, transaction_quantity, action_id, seller_trigger_type, Exchange_Price, seller_trigger_price_lower, seller_trigger_price_upper, seller_expiration_time_date, seller_expiration_time_daily);
            }

            // if there is remaining quantity, replace the order in place with a fresh one carrying a new order id and the leftover quantity
            if (buyer_quantity > transaction_quantity){
                ID new_buyer_order_id = get_database().get_new_order_id();
                add_order_to_client_pending_orders(buyer_client_id, new_buyer_order_id, buyer_order_time_date, buyer_order_time_daily, Order_Type::BUY, buyer_quantity - transaction_quantity, action_id, buyer_trigger_type, buyer_price, buyer_trigger_price_lower, buyer_trigger_price_upper, buyer_expiration_time_date, buyer_expiration_time_daily);
                buy_order = std::make_unique<Order>(new_buyer_order_id, Database);
            }
            else {
                buy_index++; // fully filled: move on to the next buy order
            }

            if (seller_quantity > transaction_quantity){
                ID new_seller_order_id = get_database().get_new_order_id();
                add_order_to_client_pending_orders(seller_client_id, new_seller_order_id, seller_order_time_date, seller_order_time_daily, Order_Type::SELL, seller_quantity - transaction_quantity, action_id, seller_trigger_type, seller_price, seller_trigger_price_lower, seller_trigger_price_upper, seller_expiration_time_date, seller_expiration_time_daily);
                sell_order = std::make_unique<Order>(new_seller_order_id, Database);
            }
            else {
                sell_index++; // fully filled: move on to the next sell order
            }
        }

        // every order strictly before buy_index / sell_index was fully filled and closed out above, erase just that (already-processed) prefix
        orders.erase(orders.begin(), orders.begin() + static_cast<long>(buy_index));
        sell_orders_for_action.erase(sell_orders_for_action.begin(), sell_orders_for_action.begin() + static_cast<long>(sell_index));
    }
}

// process continuous trading, transactions between buyers and sellers, for the action whose order triggered this call
// this sweeps only the order book for the action that just had an order added, and executes any possible transactions for that action
void Market::process_continuous_trading(const ID& action_id)
{
    auto buy_it = Buy_Orders.find(action_id);
    auto sell_it = Sell_Orders.find(action_id);
    if (buy_it == Buy_Orders.end() || sell_it == Sell_Orders.end() || buy_it->second.empty() || sell_it->second.empty()){
        return;
    }

    auto& orders = buy_it->second;
    auto& sell_orders_for_action = sell_it->second;

    std::size_t buy_index = 0, sell_index = 0;
    while (buy_index < orders.size() && sell_index < sell_orders_for_action.size()){
        auto& buy_order = orders[buy_index];
        auto& sell_order = sell_orders_for_action[sell_index];

        if (buy_order->get_price() < sell_order->get_price()){
            // same monotonicity argument as process_fixing(): both sides are kept sorted
            // best-price-first, so once the best remaining pair fails to cross, nothing further
            // down either list can cross either
            break;
        }

        // buyer 
        std::string buyer_order_info = buy_order->get_order_info();
        std::istringstream buyer_stream(buyer_order_info);
        ID buyer_order_id, buyer_order_time_date, buyer_order_time_daily, buyer_client_id, buyer_expiration_time_date, buyer_expiration_time_daily;
        std::string buyer_trigger_type_str;
        int buyer_quantity;
        double buyer_price, buyer_trigger_price_lower, buyer_trigger_price_upper;
        // extract the values
        buyer_stream >> buyer_order_id 
                        >> buyer_order_time_date 
                        >> buyer_order_time_daily 
                        >> buyer_client_id 
                        >> buyer_quantity 
                        >> buyer_trigger_type_str 
                        >> buyer_price 
                        >> buyer_trigger_price_lower 
                        >> buyer_trigger_price_upper 
                        >> buyer_expiration_time_date 
                        >> buyer_expiration_time_daily;
        Order_Trigger buyer_trigger_type = string_to_trigger(buyer_trigger_type_str);

        // seller
        std::string seller_order_info = sell_order->get_order_info();
        std::istringstream seller_stream(seller_order_info);
        ID seller_order_id, seller_order_time_date, seller_order_time_daily, seller_client_id, seller_expiration_time_date, seller_expiration_time_daily;
        std::string seller_trigger_type_str;
        int seller_quantity;
        double seller_price, seller_trigger_price_lower, seller_trigger_price_upper;
        // extract the values
        seller_stream >> seller_order_id 
                        >> seller_order_time_date 
                        >> seller_order_time_daily 
                        >> seller_client_id 
                        >> seller_quantity 
                        >> seller_trigger_type_str 
                        >> seller_price 
                        >> seller_trigger_price_lower 
                        >> seller_trigger_price_upper 
                        >> seller_expiration_time_date 
                        >> seller_expiration_time_daily;
        Order_Trigger seller_trigger_type = string_to_trigger(seller_trigger_type_str);

        // check if clients exist
        if (!client_exists(buyer_client_id) || !client_exists(seller_client_id)){
            std::cerr << "Error: One of the clients does not exist in the market.\n";
            break;
        }

        if (buyer_quantity <= 0 || seller_quantity <= 0){
            break;
        }

        // perform transaction between buyer and seller
        int transaction_quantity = std::min(buyer_quantity, seller_quantity);
        Exchange_Price = resolve_execution_price(action_id, buyer_trigger_type, seller_trigger_type, buyer_price, seller_price);
        Time exchange_time = get_current_time_ms();
        ID exchange_time_daily = get_daily_time(exchange_time);
        ID exchange_time_date = get_date_time(exchange_time);

        // update the client's portfolio
        update_client_portfolio(buyer_client_id, Order_Type::BUY, action_id, transaction_quantity, Exchange_Price, exchange_time_daily, exchange_time_date);
        update_client_portfolio(seller_client_id, Order_Type::SELL, action_id, transaction_quantity, Exchange_Price, exchange_time_daily, exchange_time_date);

        // log transaction details
        std::string transaction_details = fmt::format(
            "Transaction of {} actions {} at the price of {}$ between buyer {} and seller {} at time {}",
            transaction_quantity, 
            action_id, 
            Exchange_Price, 
            buyer_client_id, 
            seller_client_id, 
            time_to_string(exchange_time)
        );
        Message transaction_message(get_database().get_new_message_id(), get_database());
        transaction_message.log_message(
            0, 
            Message::Sender::SERVER_MESSAGE, 
            Message::Type::TRANSACTION, 
            transaction_details,
            exchange_time
        );
        // push the notification to whichever client is concerned, even if the match happened on another client's thread
        if (Notify_Client){
            Notify_Client(buyer_client_id, fmt::format(
                "TRANSACTION: bought {} of action {} at {}$ at time {}",
                transaction_quantity, action_id, Exchange_Price, time_to_string(exchange_time)
            ));
            Notify_Client(seller_client_id, fmt::format(
                "TRANSACTION: sold {} of action {} at {}$ at time {}",
                transaction_quantity, action_id, Exchange_Price, time_to_string(exchange_time)
            ));
        }
    
        // the filled portion always closes out both orders in the database (see the matching
        // comment in process_fixing() above for why a fresh order id is opened for any leftover)
        remove_order_from_client_pending_orders(buyer_client_id, buyer_order_id);
        remove_order_from_client_pending_orders(seller_client_id, seller_order_id);
        
        // add executed portion to completed orders
        if (transaction_quantity > 0){
            add_order_to_client_completed_orders(buyer_client_id, get_database().get_new_order_id(), exchange_time_date, exchange_time_daily, Order_Type::BUY, transaction_quantity, action_id, buyer_trigger_type, Exchange_Price, buyer_trigger_price_lower, buyer_trigger_price_upper, buyer_expiration_time_date, buyer_expiration_time_daily);
            add_order_to_client_completed_orders(seller_client_id, get_database().get_new_order_id(), exchange_time_date, exchange_time_daily, Order_Type::SELL, transaction_quantity, action_id, seller_trigger_type, Exchange_Price, seller_trigger_price_lower, seller_trigger_price_upper, seller_expiration_time_date, seller_expiration_time_daily);
        }

        // same in-place-replacement trick as process_fixing(): the leftover keeps the exact
        // same price-time priority, so writing it back into the same slot keeps the vector
        // sorted without a re-sort
        if (buyer_quantity > transaction_quantity){
            ID new_buyer_order_id = get_database().get_new_order_id();
            add_order_to_client_pending_orders(buyer_client_id, new_buyer_order_id, buyer_order_time_date, buyer_order_time_daily, Order_Type::BUY, buyer_quantity-transaction_quantity, action_id, buyer_trigger_type, buyer_price, buyer_trigger_price_lower, buyer_trigger_price_upper, buyer_expiration_time_date, buyer_expiration_time_daily);
            buy_order = std::make_unique<Order>(new_buyer_order_id, Database);
        }
        else {
            buy_index++;
        }

        if (seller_quantity > transaction_quantity){
            ID new_seller_order_id = get_database().get_new_order_id();
            add_order_to_client_pending_orders(seller_client_id, new_seller_order_id, seller_order_time_date, seller_order_time_daily, Order_Type::SELL, seller_quantity-transaction_quantity, action_id, seller_trigger_type, seller_price, seller_trigger_price_lower, seller_trigger_price_upper, seller_expiration_time_date, seller_expiration_time_daily);
            sell_order = std::make_unique<Order>(new_seller_order_id, Database);
        }
        else {
            sell_index++;
        }
    }

    orders.erase(orders.begin(), orders.begin() + static_cast<long>(buy_index));
    sell_orders_for_action.erase(sell_orders_for_action.begin(), sell_orders_for_action.begin() + static_cast<long>(sell_index));
}

// string representation methods 
// get the orders info as a string : order_time_date order_time_daily client_name order_type quantity action_name trigger_type price trigger_price_lower trigger_price_upper expiration_time_date expiration_time_daily,... (BUY then SELL orders)
std::string Market::get_orders_info() const
{    
    // getting the buy orders
    std::string buy_query = R"(SELECT o.order_time_date, o.order_time_daily, c.name, o.order_type, o.quantity, a.name, o.trigger_type, o.price, o.trigger_price_lower, o.trigger_price_upper, o.expiration_time_date, o.expiration_time_daily 
                            FROM orders o JOIN actions a ON o.action_id = a.action_id JOIN clients c ON o.client_id = c.client_id
                            WHERE o.order_type = 'BUY' AND (o.order_status = 'PENDING' or o.order_status = 'WAITING'))";
                            // ORDER BY o.price DESC, o.time DESC)";
    std::vector<std::vector<std::string>> buy_orders_info = Database.execute_SQL_query_vec_strings(buy_query);

    // getting the sell orders
    std::string sell_query = R"(SELECT o.order_time_date, o.order_time_daily, c.name, o.order_type, o.quantity, a.name, o.trigger_type, o.price, o.trigger_price_lower, o.trigger_price_upper, o.expiration_time_date, o.expiration_time_daily 
                            FROM orders o JOIN actions a ON o.action_id = a.action_id JOIN clients c ON o.client_id = c.client_id
                            WHERE o.order_type = 'SELL' AND (o.order_status = 'PENDING' or o.order_status = 'WAITING'))";
                            // ORDER BY o.price ASC, o.time DESC)";
    std::vector<std::vector<std::string>> sell_orders_info = Database.execute_SQL_query_vec_strings(sell_query);

    // join the buy and sell orders info into a single string
    std::string result;
    // process buy orders
    if (!buy_orders_info.empty()){
        for (int i=0; i<buy_orders_info.size(); i++){
            if (buy_orders_info[i].size() >= 12){
                result += fmt::format(
                    "{} {} {} {} {} {} {} {} {} {},",
                    two_times_to_string(std::stoll(buy_orders_info[i][0]), std::stoll(buy_orders_info[i][1])),
                    buy_orders_info[i][2],
                    buy_orders_info[i][3],
                    buy_orders_info[i][4],
                    buy_orders_info[i][5],
                    buy_orders_info[i][6],
                    buy_orders_info[i][7],
                    buy_orders_info[i][8],
                    buy_orders_info[i][9],
                    two_times_to_string(std::stoll(buy_orders_info[i][10]), std::stoll(buy_orders_info[i][11]))
                );
            }
        }
    }
    // process sell orders
    if (!sell_orders_info.empty()){
        for (int i=0; i<sell_orders_info.size(); i++){
            if (sell_orders_info[i].size() >= 12){
                result += fmt::format(
                    "{} {} {} {} {} {} {} {} {} {},",
                    two_times_to_string(std::stoll(sell_orders_info[i][0]), std::stoll(sell_orders_info[i][1])),
                    sell_orders_info[i][2],
                    sell_orders_info[i][3],
                    sell_orders_info[i][4],
                    sell_orders_info[i][5],
                    sell_orders_info[i][6],
                    sell_orders_info[i][7],
                    sell_orders_info[i][8],
                    sell_orders_info[i][9],
                    two_times_to_string(std::stoll(sell_orders_info[i][10]), std::stoll(sell_orders_info[i][11]))
                );
            }
        }
    }
    if (!result.empty()){
        result.pop_back(); // remove trailing comma
    }
    return result;
}

// get the actions info as a string : action_name quantity last_price time,...
std::string Market::get_actions_info() const
{
    std::string query = R"(SELECT a.name, a.quantity, p.price, p.daily_time, p.date_time
        FROM actions a LEFT JOIN prices p ON a.action_id = p.action_id
        WHERE (p.date_time, p.daily_time) = (
            SELECT p2.date_time, p2.daily_time
            FROM prices p2
            WHERE p2.action_id = a.action_id
            AND p2.date_time = (
                SELECT MAX(date_time)
                FROM prices
                WHERE action_id = p2.action_id
            )
            AND p2.daily_time = (
                SELECT MAX(daily_time)
                FROM prices
                WHERE action_id = p2.action_id
                AND date_time = p2.date_time
            )
        )
    )";
    std::vector<std::vector<std::string>> market_action_info = Database.execute_SQL_query_vec_strings(query);

    // check if we have enough data before accessing elements
    if (market_action_info.empty() || market_action_info[0].size() < 5){
        return "";
    }

    // join the action info into a single string
    std::string result;
    for (const auto& action_info : market_action_info){
        if (action_info.size() >= 5){
            result += fmt::format(
                "{} {} {} {},",
                action_info[0],
                action_info[1], 
                action_info[2],
                two_times_to_string(std::stoll(action_info[3]), std::stoll(action_info[4]))
            );
        }
    }
    if (!result.empty()){
        result.pop_back(); // remove trailing comma
    }
    return result;
}

// get the market info as a string : market_value;order_time_date order_time_daily client_name order_type quantity action_name trigger_type price trigger_price_lower trigger_price_upper expiration_time_date expiration_time_daily,... (BUY then SELL orders);action_name quantity last_price time,...
std::string Market::get_market_info() const
{
    std::string result = fmt::format(
        "{};{};{}",
        get_market_value(), 
        get_orders_info(), 
        get_actions_info()  
    );
    return result;
}

