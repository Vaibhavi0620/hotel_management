// cancel.cpp
// Compile: g++ cancel.cpp -o cancel.exe -lsqlite3

#include <iostream>
#include <string>
#include <sqlite3.h>
#include <cstdlib>

static std::string url_decode(const std::string &s){
    std::string out; out.reserve(s.size());
    for (size_t i=0;i<s.size();++i){
        char c = s[i];
        if (c=='+') out.push_back(' ');
        else if (c=='%' && i+2 < s.size()){
            std::string h = s.substr(i+1,2);
            char ch = (char) strtol(h.c_str(), nullptr, 16);
            out.push_back(ch);
            i += 2;
        } else out.push_back(c);
    }
    return out;
}

static std::string html_escape(const std::string &s){
    std::string o;
    for(char c:s){
        switch(c){
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            case '\'': o += "&#39;"; break;
            default: o.push_back(c);
        }
    }
    return o;
}

static std::string get_env(const char *k){
    const char* v = getenv(k);
    return v ? v : "";
}

static std::string get_post_body(){
    int len = 0;
    std::string cl = get_env("CONTENT_LENGTH");
    if (!cl.empty()) len = atoi(cl.c_str());
    if (len <= 0) return std::string();
    std::string body; body.resize(len);
    std::cin.read(&body[0], len);
    return body;
}

int main(){
    std::cout << "Content-Type: text/html\r\n\r\n";

    std::string method = get_env("REQUEST_METHOD");
    if (method == "GET"){
        std::cout << "<!doctype html><html><head><meta charset='utf-8'><title>Cancel Booking</title>"
                  << "<link rel='stylesheet' href='/styles.css'></head><body>"
                  << "<header class='topbar'><h1>Cancel Booking</h1></header><main class='center'>"
                  << "<div class='card'><form method='POST' action='/cgi-bin/cancel.exe'>"
                  << "<label>Booking ID:</label><br><input name='booking_id' type='number' required><br>"
                  << "<button type='submit' class='btn'>Cancel Booking</button>"
                  << "</form></div></main><footer class='footer'><a href='/index.html'>Home</a></footer></body></html>";
        return 0;
    }

    // POST
    std::string body = get_post_body();
    std::string booking_id_s;
    size_t pos = 0;
    while (pos < body.size()){
        size_t eq = body.find('=', pos);
        if (eq == std::string::npos) break;
        std::string key = body.substr(pos, eq-pos);
        size_t amp = body.find('&', eq+1);
        std::string val = (amp==std::string::npos) ? body.substr(eq+1) : body.substr(eq+1, amp-(eq+1));
        if (key=="booking_id") booking_id_s = url_decode(val);
        if (amp==std::string::npos) break;
        pos = amp+1;
    }

    if (booking_id_s.empty()){
        std::cout << "<h2>Missing booking id</h2><p><a href='/cgi-bin/cancel.exe'>Back</a></p>";
        return 0;
    }
    int booking_id = atoi(booking_id_s.c_str());

    sqlite3 *db = nullptr;
    if (sqlite3_open("C:\\xampp\\htdocs\\hotel.db", &db) != SQLITE_OK) {
        std::cout << "<h2>Cannot open DB</h2>";
        return 0;
    }

    // find booking and status
    const char *q = "SELECT room_id, status FROM bookings WHERE booking_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, q, -1, &stmt, nullptr) != SQLITE_OK){
        std::cout << "<h2>DB error</h2>";
        sqlite3_close(db);
        return 0;
    }
    sqlite3_bind_int(stmt, 1, booking_id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW){
        sqlite3_finalize(stmt);
        std::cout << "<h2>Booking not found</h2><p><a href='/cgi-bin/cancel.exe'>Back</a></p>";
        sqlite3_close(db);
        return 0;
    }
    int room_id = sqlite3_column_int(stmt, 0);
    const unsigned char *st = sqlite3_column_text(stmt, 1);
    std::string status = st ? reinterpret_cast<const char*>(st) : "";
    sqlite3_finalize(stmt);

    if (status == "cancelled"){
        std::cout << "<h2>Booking already cancelled</h2><p><a href='/cgi-bin/rooms.exe'>View Rooms</a></p>";
        sqlite3_close(db);
        return 0;
    }

    // transaction: update booking to cancelled and mark room available
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const char *upd1 = "UPDATE bookings SET status='cancelled' WHERE booking_id = ?;";
    sqlite3_stmt *u1 = nullptr;
    if (sqlite3_prepare_v2(db, upd1, -1, &u1, nullptr) == SQLITE_OK){
        sqlite3_bind_int(u1, 1, booking_id);
        sqlite3_step(u1);
        sqlite3_finalize(u1);
    }

    const char *upd2 = "UPDATE rooms SET is_available = 1 WHERE room_id = ?;";
    sqlite3_stmt *u2 = nullptr;
    if (sqlite3_prepare_v2(db, upd2, -1, &u2, nullptr) == SQLITE_OK){
        sqlite3_bind_int(u2, 1, room_id);
        sqlite3_step(u2);
        sqlite3_finalize(u2);
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);

    std::cout << "<!doctype html><html><head><meta charset='utf-8'><title>Cancelled</title>"
              << "<link rel='stylesheet' href='/styles.css'></head><body>"
              << "<div class='card'><h2>Booking Cancelled</h2>"
              << "<p>Booking ID: <strong>" << booking_id << "</strong></p>"
              << "<p>Room: <strong>" << room_id << "</strong> is now available.</p>"
              << "<a class='btn' href='/cgi-bin/rooms.exe'>View Rooms</a> "
              << "<a class='btn outline' href='/index.html'>Home</a>"
              << "</div></body></html>";

    return 0;
}
