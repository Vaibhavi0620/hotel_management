#include <iostream>
#include <sqlite3.h>

int main() {
    sqlite3* db;
    sqlite3_open("hotel.db", &db);

    const char* sql = "SELECT room_id, type, price, is_available FROM rooms;";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

    std::cout << "Content-type: text/html\n\n";
    std::cout << "<html><body><h1>Rooms</h1><table border='1'>";
    std::cout << "<tr><th>ID</th><th>Type</th><th>Price</th><th>Available</th></tr>";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::cout << "<tr>";
        std::cout << "<td>" << sqlite3_column_int(stmt, 0) << "</td>";
        std::cout << "<td>" << sqlite3_column_text(stmt, 1) << "</td>";
        std::cout << "<td>" << sqlite3_column_int(stmt, 2) << "</td>";
        std::cout << "<td>" << sqlite3_column_int(stmt, 3) << "</td>";
        std::cout << "</tr>";
    }

    std::cout << "</table></body></html>";

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}
