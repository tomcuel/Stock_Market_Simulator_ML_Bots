//==========================================================================
// File that defines the database management class
//==========================================================================
#ifndef DATABASE_MANAGEMENT_HPP
#define DATABASE_MANAGEMENT_HPP
#include "utility.hpp"


class Database_Manager
{
private:
    sqlite3* Database;
public:
    // constructor
    Database_Manager(const std::string& database_name);
    // destructor
    void close_database();

    // getters
    sqlite3* get_database() const;
  
    // functions to execute an SQL query
    void execute_SQL(const std::string& sql); // modify the database
    int execute_SQL_query_int(const std::string& sql); // get an integer result from the database
    std::vector<int> execute_SQL_query_ints(const std::string& query); // get a vector of integers from the database
    ID execute_SQL_query_ID(const std::string& sql); // get an ID result from the database
    std::vector<ID> execute_SQL_query_IDs(const std::string& query); // get a vector of IDs from the database
    ID get_new_order_id(); // return an ID that is not already in the orders table
    ID get_new_action_id(); // return an ID that is not already in the actions table
    ID get_new_message_id(); // return an ID that is not already in the messages table
    double execute_SQL_query_double(const std::string& sql); // get a double result from the database
    std::vector<double> execute_SQL_query_doubles(const std::string& query); // get a vector of doubles from the database
    std::string execute_SQL_query_string(const std::string& sql); // get a string result from the database
    std::vector<std::string> execute_SQL_query_strings(const std::string& query); // get a vector of strings from the database
    std::vector<std::vector<std::string>> execute_SQL_query_vec_strings(const std::string& query); // get a vector of vectors of strings from the database
    std::vector<unsigned char> execute_SQL_query_blob(const std::string& sql); // get a blob result from the database
    std::vector<std::vector<unsigned char>> execute_SQL_query_blobs(const std::string& query); // get a vector of blobs from the database

    // database management
    void create_tables(); // create the tables in the databases
    void reset_database(); // reset all the datas in the database to have a clear market
    void reset_database_action_prices(const ID& reset_daily_time, const ID& reset_date_time); // reset the prices in the database to the actions of the market and the client's portfolio, to the last price and the given time
    void reset_database_messages(); // function to reset the log of the messages
};


#endif // DATABASE_MANAGEMENT_HPP