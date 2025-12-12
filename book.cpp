// book.cpp
// Compile: g++ book.cpp -o book.exe -lsqlite3

#include <iostream>
#include <string>
#include <sstream>
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

    // determine method and possible room_id in query string
    std::string method = get_env("REQUEST_METHOD");
    std::string q = get_env("QUERY_STRING");
    std::string q_room;
    if (!q.empty()){
        // parse query like room_id=101
        size_t p = q.find("room_id=");
        if (p!=std::string::npos){
            q_room = q.substr(p+8);
            // strip ampersand if any
            size_t a = q_room.find('&');
            if (a!=std::string::npos) q_room = q_room.substr(0,a);
        }
    }

    if (method == "GET") {
        // show form (prefill room id if present)
        std::cout << "<!doctype html><html><head><meta charset='utf-8'><title>Book Room</title>"
                  << "<link rel='stylesheet' href='/styles.css'></head><body>"
                  << "<header class='topbar'><h1>Book Room</h1></header><main class='center'>"
                  << "<div class='card'><form method='POST' action='/cgi-bin/book.exe'>";

        std::cout << "<label>Room ID:</label><br>";
        std::cout << "<input name='room_id' type='number' value='" << html_escape(q_room) << "' required readonly><br>";
        std::cout << "<label>Name:</label><br><input name='name' type='text' required><br>";
        std::cout << "<label>Phone (optional):</label><br><input name='phone' type='text'><br>";
        std::cout << "<label>Check-in:</label><br><input name='check_in' type='date'><br>";
        std::cout << "<label>Check-out:</label><br><input name='check_out' type='date'><br>";
        std::cout << "<button type='submit' class='btn'>Confirm Booking</button>";
        std::cout << "</form></div></main><footer class='footer'><a href='/cgi-bin/rooms.exe'>Back</a></footer></body></html>";
        return 0;
    }

    // POST -> perform booking
    std::string body = get_post_body();
    // parse urlencoded form: name=abc&room_id=101...
    std::string name, phone, room_id_s, check_in, check_out;
    size_t pos = 0;
    while (pos < body.size()){
        size_t eq = body.find('=', pos);
        if (eq == std::string::npos) break;
        std::string key = body.substr(pos, eq-pos);
        size_t amp = body.find('&', eq+1);
        std::string val = (amp==std::string::npos) ? body.substr(eq+1) : body.substr(eq+1, amp-(eq+1));
        if (key=="name") name = url_decode(val);
        else if (key=="phone") phone = url_decode(val);
        else if (key=="room_id") room_id_s = url_decode(val);
        else if (key=="check_in") check_in = url_decode(val);
        else if (key=="check_out") check_out = url_decode(val);
        if (amp==std::string::npos) break;
        pos = amp+1;
    }

    if (name.empty() || room_id_s.empty()){
        std::cout << "<h2>Missing fields. Name and Room ID are required.</h2><p><a href='/cgi-bin/book.exe'>Back</a></p>";
        return 0;
    }

    int room_id = atoi(room_id_s.c_str());

    sqlite3 *db = nullptr;
    if (sqlite3_open("C:\\xampp\\htdocs\\hotel.db", &db) != SQLITE_OK) {
        std::cout << "<h2>Cannot open DB</h2>";
        return 0;
    }

    // check availability
    const char *chk_sql = "SELECT is_available FROM rooms WHERE room_id = ?;";
    sqlite3_stmt *chk = nullptr;
    if (sqlite3_prepare_v2(db, chk_sql, -1, &chk, nullptr) != SQLITE_OK){
        std::cout << "<h2>Database error</h2>";
        sqlite3_close(db);
        return 0;
    }
    sqlite3_bind_int(chk, 1, room_id);
    int rc = sqlite3_step(chk);
    if (rc != SQLITE_ROW){
        sqlite3_finalize(chk);
        std::cout << "<h2>Room not found.</h2><p><a href='/cgi-bin/rooms.exe'>Back</a></p>";
        sqlite3_close(db);
        return 0;
    }
    int avail = sqlite3_column_int(chk,0);
    sqlite3_finalize(chk);
    if (!avail){
        std::cout << "<h2>Room is not available.</h2><p><a href='/cgi-bin/rooms.exe'>Back</a></p>";
        sqlite3_close(db);
        return 0;
    }

    // begin transaction
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // insert booking
    const char *ins_sql = "INSERT INTO bookings (customer_name, phone, room_id, check_in, check_out, status) VALUES (?, ?, ?, ?, ?, 'active');";
    sqlite3_stmt *ins = nullptr;
    if (sqlite3_prepare_v2(db, ins_sql, -1, &ins, nullptr) != SQLITE_OK){
        std::cout << "<h2>DB error (prepare insert)</h2>";
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return 0;
    }
    sqlite3_bind_text(ins, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 2, phone.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(ins, 3, room_id);
    sqlite3_bind_text(ins, 4, check_in.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins, 5, check_out.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(ins) != SQLITE_DONE){
        sqlite3_finalize(ins);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        std::cout << "<h2>Failed to save booking.</h2><p><a href='/cgi-bin/rooms.exe'>Back</a></p>";
        sqlite3_close(db);
        return 0;
    }
    sqlite3_finalize(ins);

    int booking_id = (int)sqlite3_last_insert_rowid(db);

    // mark room not available
    const char *upd_sql = "UPDATE rooms SET is_available = 0 WHERE room_id = ?;";
    sqlite3_stmt *upd = nullptr;
    if (sqlite3_prepare_v2(db, upd_sql, -1, &upd, nullptr) == SQLITE_OK){
        sqlite3_bind_int(upd, 1, room_id);
        sqlite3_step(upd);
        sqlite3_finalize(upd);
    }

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);

    std::cout << "<!doctype html><html><head><meta charset='utf-8'><title>Booked</title>"
              << "<link rel='stylesheet' href='/styles.css'></head><body>"
              << "<div class='card'><h2>Booking Successful!</h2>"
              << "<p>Booking ID: <strong>" << booking_id << "</strong></p>"
              << "<p>Room: <strong>" << room_id << "</strong></p>"
              << "<p>Name: " << html_escape(name) << "</p>"
              << "<a class='btn' href='/cgi-bin/rooms.exe'>View Rooms</a> "
              << "<a class='btn outline' href='/index.html'>Home</a>"
              << "</div></body></html>";

    return 0;
}
