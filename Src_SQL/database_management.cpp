#include "database_management.hpp"


// constructor
Database_Manager::Database_Manager(const std::string& database_name)
{
    if (sqlite3_open(database_name.c_str(), &Database) != SQLITE_OK){
        std::cerr << "Error opening database: " << sqlite3_errmsg(Database) << std::endl;
        throw std::runtime_error("Error opening database");
    }
}

// destructor
void Database_Manager::close_database()
{
    sqlite3_close(Database);
}


// getters
sqlite3* Database_Manager::get_database() const
{
    return Database;
}


// functions to execute an SQL query
// modify the database
void Database_Manager::execute_SQL(const std::string& sql)
{
    char* error_message = nullptr;
    if (sqlite3_exec(Database, sql.c_str(), nullptr, nullptr, &error_message) != SQLITE_OK){
        std::cerr << "Error executing SQL: " << error_message << std::endl;
        sqlite3_free(error_message);
    }
}

// get an integer result from the database
int Database_Manager::execute_SQL_query_int(const std::string& sql)
{
    sqlite3_stmt* stmt;
    int result = -1; // default if no result

    if (sqlite3_prepare_v2(Database, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);  // get the first column value
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

// get a vector of integers from the database
std::vector<int> Database_Manager::execute_SQL_query_ints(const std::string& query)
{
    std::vector<int> ints;
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(Database, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ints.push_back(sqlite3_column_int(stmt, 0)); // get the first column value
        }
    }
    sqlite3_finalize(stmt);
    return ints;
}

// get an ID result from the database
ID Database_Manager::execute_SQL_query_ID(const std::string& sql)
{
    sqlite3_stmt* stmt;
    ID result = -1; // default if no result

    if (sqlite3_prepare_v2(Database, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_int(stmt, 0);  // get the first column value
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

// get a vector of IDs from the database
std::vector<ID> Database_Manager::execute_SQL_query_IDs(const std::string& query)
{
    std::vector<ID> ids;
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(Database, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ids.push_back(sqlite3_column_int64(stmt, 0)); // get the first column value
        }
    }
    sqlite3_finalize(stmt);
    return ids;
}

// return an ID that is not already in the orders table
ID Database_Manager::get_new_order_id()
{
    ID new_order = generate_random_uint32();
    std::string query = fmt::format(
        "SELECT order_id FROM orders WHERE order_id = {}",
        new_order
    );
    while (execute_SQL_query_ID(query) != -1){
        new_order = generate_random_uint32();
    }
    return new_order;
}

// return an ID that is not already in the actions table
ID Database_Manager::get_new_action_id()
{
    ID new_action = generate_random_uint32();
    std::string query = fmt::format(
        "SELECT action_id FROM actions WHERE action_id = {}",
        new_action
    );
    while (execute_SQL_query_ID(query) != -1){
        new_action = generate_random_uint32();
    }
    return new_action;
}

// return an ID that is not already in the messages table
ID Database_Manager::get_new_message_id()
{
    ID new_message = generate_random_uint32();
    std::string query = fmt::format(
        "SELECT message_id FROM messages WHERE message_id = {}",
        new_message
    );
    while (execute_SQL_query_ID(query) != -1){
        new_message = generate_random_uint32();
    }
    return new_message;
}

// get a double result from the database
double Database_Manager::execute_SQL_query_double(const std::string& sql)
{
    sqlite3_stmt* stmt;
    double result = -1.0; // default if no result

    if (sqlite3_prepare_v2(Database, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = sqlite3_column_double(stmt, 0);  // get the first column value
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

// get a vector of doubles from the database
std::vector<double> Database_Manager::execute_SQL_query_doubles(const std::string& query)
{
    std::vector<double> doubles;
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(Database, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            doubles.push_back(sqlite3_column_double(stmt, 0)); // get the first column value
        }
    }
    sqlite3_finalize(stmt);
    return doubles;
}

// get a string result from the database
std::string Database_Manager::execute_SQL_query_string(const std::string& sql)
{
    sqlite3_stmt* stmt;
    std::string result;

    if (sqlite3_prepare_v2(Database, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));  // get the first column value
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

// get a vector of strings from the database
std::vector<std::string> Database_Manager::execute_SQL_query_strings(const std::string& query)
{
    std::vector<std::string> strings;
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(Database, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            strings.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))); // get the first column value
        }
    }
    sqlite3_finalize(stmt);
    return strings;
}

// get a vector of vectors of strings from the database
std::vector<std::vector<std::string>> Database_Manager::execute_SQL_query_vec_strings(const std::string& query)
{
    std::vector<std::vector<std::string>> results;
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(Database, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::vector<std::string> row;
            int column_count = sqlite3_column_count(stmt);
            
            for (int i = 0; i < column_count; ++i) {
                const char* column_text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                row.push_back(column_text ? column_text : ""); // handle NULL values
            }
            results.push_back(row);
        }
    }
    sqlite3_finalize(stmt);
    return results;
}

// get a blob result from the database
std::vector<unsigned char> Database_Manager::execute_SQL_query_blob(const std::string& query)
{
    sqlite3_stmt* stmt;
    std::vector<unsigned char> result;

    if (sqlite3_prepare_v2(Database, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK){
        if (sqlite3_step(stmt) == SQLITE_ROW){
            const void* data = sqlite3_column_blob(stmt, 0);  // retrieve the first column as BLOB
            int length = sqlite3_column_bytes(stmt, 0);  // get the size of the BLOB
            result.assign(reinterpret_cast<const unsigned char*>(data), 
                          reinterpret_cast<const unsigned char*>(data) + length);  // convert BLOB to vector<unsigned char>
        }
    }
    sqlite3_finalize(stmt);
    return result;
}

// get a vector of blobs from the database
std::vector<std::vector<unsigned char>> Database_Manager::execute_SQL_query_blobs(const std::string& query)
{
    std::vector<std::vector<unsigned char>> blobs;
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(Database, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK){
        while (sqlite3_step(stmt) == SQLITE_ROW){
            const void* data = sqlite3_column_blob(stmt, 0);  // retrieve the first column as BLOB
            int length = sqlite3_column_bytes(stmt, 0);  // get the size of the BLOB
            std::vector<unsigned char> blob(reinterpret_cast<const unsigned char*>(data), 
                                            reinterpret_cast<const unsigned char*>(data) + length);  // convert BLOB to vector<unsigned char>
            blobs.push_back(blob);
        }
    }
    sqlite3_finalize(stmt);
    return blobs;
}


// database management
// function to create the tables in the database
void Database_Manager::create_tables()
{
    // SQL query to create the "actions" table
    std::string create_actions_table = R"(
        CREATE TABLE IF NOT EXISTS actions (
            action_id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            quantity INTEGER NOT NULL
        );
    )";
    execute_SQL(create_actions_table);

    // SQL query to create the "prices" table
    std::string create_prices_table = R"(
        CREATE TABLE IF NOT EXISTS prices (
            price_id INTEGER PRIMARY KEY,
            action_id INTEGER NOT NULL,
            price REAL NOT NULL,
            date_time INTEGER NOT NULL,
            daily_time INTEGER NOT NULL,
            FOREIGN KEY (action_id) REFERENCES actions(action_id)
        );
    )";
    execute_SQL(create_prices_table);

    // SQL query to create the "clients" table
    std::string create_clients_table = R"(
        CREATE TABLE IF NOT EXISTS clients (
            client_id INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            encrypted_password BLOB NOT NULL,
            balance REAL NOT NULL
        );
    )";
    execute_SQL(create_clients_table);

    // SQL query to create the "orders" table
    std::string create_orders_table = R"(
        CREATE TABLE IF NOT EXISTS orders (
            order_id INTEGER PRIMARY KEY,
            order_status TEXT NOT NULL,                -- PENDING or COMPLETED or WAITING (for LIMIT/STOP/LIMIT_STOP orders that have not yet reached their trigger condition)
            order_time_date INTEGER NOT NULL,
            order_time_daily INTEGER NOT NULL,
            client_id INTEGER NOT NULL,
            order_type TEXT NOT NULL,                  -- BUY or SELL
            quantity INTEGER NOT NULL,
            action_id INTEGER NOT NULL,
            trigger_type TEXT NOT NULL,                -- MARKET or LIMIT or STOP or LIMIT_STOP
            price REAL NOT NULL,                       -- depends on the trigger type
            trigger_price_lower REAL NOT NULL,         -- depends on the trigger type
            trigger_price_upper REAL NOT NULL,         -- depends on the trigger type
            expiration_time_date INTEGER NOT NULL,     -- UINT16_MAX=max_number if no expiration 
            expiration_time_daily INTEGER NOT NULL,    -- UINT16_MAX=max_number if no expiration
            FOREIGN KEY (client_id) REFERENCES clients(client_id),
            FOREIGN KEY (action_id) REFERENCES actions(action_id)
        );
    )";
    execute_SQL(create_orders_table);

    // SQL query to create the "client_portfolio" table
    std::string create_client_portfolio_table = R"(
        CREATE TABLE IF NOT EXISTS client_portfolio (
            client_id INTEGER NOT NULL,
            action_id INTEGER NOT NULL,
            quantity INTEGER NOT NULL,
            PRIMARY KEY (client_id, action_id),
            FOREIGN KEY (client_id) REFERENCES clients(client_id),
            FOREIGN KEY (action_id) REFERENCES actions(action_id)
        );
    )";
    execute_SQL(create_client_portfolio_table);

    // SQL query to create the "messages" table (client_id = 0 for the server)
    std::string create_messages_table = R"(
        CREATE TABLE IF NOT EXISTS messages (
            message_id INTEGER PRIMARY KEY,
            client_id INTEGER NOT NULL,
            message_sender TEXT NOT NULL,
            message_type TEXT NOT NULL,
            content TEXT NOT NULL,  
            date_time INTEGER NOT NULL,
            daily_time INTEGER NOT NULL,
            FOREIGN KEY (client_id) REFERENCES clients(client_id)
        );
    )";
    execute_SQL(create_messages_table);

    // SQL query to create the "encryption_keys" table
    std::string create_encryption_keys_table = R"(
        CREATE TABLE encryption_keys (
            id INT AUTO_INCREMENT PRIMARY KEY,
            key BLOB,
            iv BLOB
        );
    )";
    execute_SQL(create_encryption_keys_table);
}

// reset all the datas in the database to have a clear market
void Database_Manager::reset_database()
{
    // drop tables
    execute_SQL("DROP TABLE IF EXISTS actions;");
    execute_SQL("DROP TABLE IF EXISTS prices;");
    execute_SQL("DROP TABLE IF EXISTS clients;");
    execute_SQL("DROP TABLE IF EXISTS orders;");
    execute_SQL("DROP TABLE IF EXISTS client_portfolio;");
    execute_SQL("DROP TABLE IF EXISTS messages;");
    execute_SQL("DROP TABLE IF EXISTS encryption_keys;");

    // create tables
    create_tables();
}

// reset the prices in the database to the actions of the market and the client's portfolio, to the last price and the given time
void Database_Manager::reset_database_action_prices(const ID& reset_daily_time, const ID& reset_date_time)
{
   // Step 1: Delete all but the most recent price for each action_id based on both date_time and daily_time
   std::string delete_old_prices_query = R"(
        DELETE FROM prices
        WHERE price_id NOT IN (
            SELECT price_id FROM (
                SELECT price_id
                FROM prices p
                WHERE (p.action_id, p.date_time, p.daily_time) IN (
                    SELECT p2.date_time, p2.daily_time
                    FROM prices p2
                    WHERE p2.action_id = p.action_id
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
            )
        );
    )";
    execute_SQL(delete_old_prices_query);

    // Step 2: update the remaining prices with the given reset_time
    std::string update_prices_query = 
        "UPDATE prices "
        "SET daily_time = " + std::to_string(reset_daily_time) + ", "
        "date_time = " + std::to_string(reset_date_time) + " "
        "WHERE (date_time, daily_time) = ("
            "SELECT p2.date_time, p2.daily_time "
            "FROM prices p2 "
            "WHERE p2.action_id = prices.action_id "
            "AND p2.date_time = ("
                "SELECT MAX(date_time) "
                "FROM prices "
                "WHERE action_id = p2.action_id "
            ") "
            "AND p2.daily_time = ("
                "SELECT MAX(daily_time) "
                "FROM prices "
                "WHERE action_id = p2.action_id "
                "AND date_time = p2.date_time "
            ") "
        ");";
    execute_SQL(update_prices_query);
}

// function to reset the log of the messages
void Database_Manager::reset_database_messages()
{
    execute_SQL("DROP TABLE IF EXISTS messages;");
    // SQL query to create the "messages" table
    std::string create_messages_table = R"(
        CREATE TABLE IF NOT EXISTS messages (
            message_id INTEGER PRIMARY KEY AUTOINCREMENT,
            client_id INTEGER NOT NULL,
            message_sender TEXT NOT NULL,
            message_type TEXT NOT NULL,
            content TEXT NOT NULL,  
            time INTEGER NOT NULL,
            FOREIGN KEY (client_id) REFERENCES clients(client_id)
        );
    )";
    execute_SQL(create_messages_table);
}

