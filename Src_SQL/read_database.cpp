#include <sqlite3.h>

#include <iostream>
#include <string>

namespace {

int print_row(void*, int column_count, char** values, char** column_names)
{
    for (int column = 0; column < column_count; ++column) {
        std::cout << column_names[column] << "="
                  << (values[column] ? values[column] : "NULL");
        if (column + 1 < column_count) {
            std::cout << " | ";
        }
    }
    std::cout << '\n';
    return 0;
}

} // namespace

int main(int argc, char* argv[])
{
    const std::string database_path = "../Data/Stock_Market_App.db";
    sqlite3* database = nullptr;

    if (sqlite3_open_v2(database_path.c_str(), &database, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        std::cerr << "Could not open " << database_path << ": " << sqlite3_errmsg(database) << '\n';
        sqlite3_close(database);
        return 1;
    }

    const char* table_query =
        "SELECT name FROM sqlite_master "
        "WHERE type = 'table' AND name NOT LIKE 'sqlite_%' "
        "ORDER BY name;";
    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(database, table_query, -1, &statement, nullptr) != SQLITE_OK) {
        std::cerr << "Could not list tables: " << sqlite3_errmsg(database) << '\n';
        sqlite3_close(database);
        return 1;
    }

    while (sqlite3_step(statement) == SQLITE_ROW) {
        const char* table_name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        if (std::string(table_name) == std::string(argv[1])) {
            std::cout << "\nSkipping table " << table_name << " as it matches the argument to skip\n";
            continue; // Skip this tables
        }
        std::cout << "\n=== " << table_name << " ===\n";
        const std::string query = "SELECT * FROM \"" + std::string(table_name) + "\";";
        char* error_message = nullptr;
        if (sqlite3_exec(database, query.c_str(), print_row, nullptr, &error_message) != SQLITE_OK) {
            std::cerr << "Could not read table " << table_name << ": " << error_message << '\n';
            sqlite3_free(error_message);
        }
    }

    sqlite3_finalize(statement);
    sqlite3_close(database);
    return 0;
}
/*
Usage: ./read_database [table_name_to_skip]
*/