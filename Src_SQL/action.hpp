//==========================================================================
// File containing the definition of what an action is 
//==========================================================================
#ifndef ACTION_HPP
#define ACTION_HPP
#include "database_management.hpp"


class Action 
{
private:
    ID Action_Id; // define the action id
    Database_Manager& Database; // reference to the database manager for queries

public:
    // constructor
    Action(const ID& action_id, Database_Manager& database); // simple init

    // getters
    ID get_action_id() const; // get the action id
    double get_current_price() const; // get the current price of the action
    
    // string representation methods
    std::string get_action_info() const; // get the action info as a string : name quantity,price1 time1,price2 time2, ...
};


#endif // ACTION_HPP