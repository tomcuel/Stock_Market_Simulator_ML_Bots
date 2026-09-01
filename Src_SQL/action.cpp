#include "action.hpp"


// constructor
Action::Action(const ID& action_id, Database_Manager& database) : Action_Id(action_id), Database(database)
{
    
}


// getters
// get the action id
ID Action::get_action_id() const
{    
    return Action_Id;
}

// get the current price of the action
double Action::get_current_price() const
{
    std::string query = fmt::format(
        "SELECT price FROM prices WHERE action_id = {} ORDER BY date_time DESC, daily_time DESC LIMIT 1",
        get_action_id()
    );
    return Database.execute_SQL_query_double(query);
}


// string representation methods
// get the action info as a string : name quantity,price1 time1,price2 time2, ...
std::string Action::get_action_info() const
{   
    std::string query = fmt::format(
        R"(SELECT a.name, a.quantity, p.price, p.date_time, p.daily_time
            FROM actions a
            LEFT JOIN prices p ON a.action_id = p.action_id
            WHERE a.action_id = {}
            ORDER BY p.date_time ASC, p.daily_time ASC)",
        get_action_id()
    );
    std::vector<std::vector<std::string>> action_info = Database.execute_SQL_query_vec_strings(query);

    // check if the result contains the necessary data
    if (action_info.empty() || action_info[0].size() < 2){
        return "";  // return empty if data is missing
    }

    std::ostringstream result;
    // first row to get the name and the quantity
    result << action_info[0][0] << " " << action_info[0][1];
    // iterate through the remaining rows for price-time pairs
    for (size_t i = 0; i < action_info.size(); ++i){
        if (action_info[i].size() >= 5){
            result << "," << action_info[i][2] << " " << two_times_to_string(std::stoll(action_info[i][3]), std::stoll(action_info[i][4]));
        }
    }
    return result.str();
}

