#define _WIN32_WINNT 0x0A00 // for Windows 10 or higher
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include "httplib.h"   // https://github.com/yhirose/cpp-httplib (single header)
#include "database.h"

// small helper: parse application/x-www-form-urlencoded body into map
static std::string url_decode(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') out.push_back(' ');
        else if (s[i] == '%' && i + 2 < s.size()) {
            std::string hex = s.substr(i + 1, 2);
            char ch = (char) strtol(hex.c_str(), nullptr, 16);
            out.push_back(ch);
            i += 2;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

static std::map<std::string, std::string> parse_form_urlencoded(const std::string &body) {
    std::map<std::string, std::string> m;
    size_t i = 0;
    while (i < body.size()) {
        size_t eq = body.find('=', i);
        if (eq == std::string::npos) break;
        std::string key = body.substr(i, eq - i);
        size_t amp = body.find('&', eq + 1);
        std::string val;
        if (amp == std::string::npos) {
            val = body.substr(eq + 1);
            i = body.size();
        } else {
            val = body.substr(eq + 1, amp - (eq + 1));
            i = amp + 1;
        }
        m[url_decode(key)] = url_decode(val);
    }
    return m;
}

int main() {
    // initialize DB
    Database db;
    const std::string dbfile = "hotel.db";
    const std::string sqlfile = "schema.sql";

    if (!db.open(dbfile, sqlfile)) {
        std::cerr << "Failed to open/init DB\n";
        return 1;
    }

    httplib::Server svr;

    // CORS preflight (optional)
    svr.Options(".*", [](const httplib::Request& req, httplib::Response &res){
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 200;
    });

    // GET /rooms -> return JSON array of rooms
    svr.Get("/rooms", [&](const httplib::Request& req, httplib::Response &res) {
        auto rooms = db.getRooms();
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < rooms.size(); ++i) {
            const auto &r = rooms[i];
            oss << "{";
            oss << "\"room_id\":" << r.room_id << ",";
            oss << "\"type\":\"" << r.type << "\",";
            oss << "\"price\":" << r.price << ",";
            oss << "\"is_available\":" << r.is_available;
            oss << "}";
            if (i + 1 < rooms.size()) oss << ",";
        }
        oss << "]";
        res.set_header("Content-Type", "application/json");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(oss.str(), "application/json");
    });

    // POST /book (x-www-form-urlencoded)
    svr.Post("/book", [&](const httplib::Request& req, httplib::Response &res){
        std::string body = req.body;
        auto form = parse_form_urlencoded(body);
        std::string name = form.count("name") ? form["name"] : "";
        int room_id = form.count("room_id") ? std::stoi(form["room_id"]) : 0;
        std::string check_in = form.count("check_in") ? form["check_in"] : "";
        std::string check_out = form.count("check_out") ? form["check_out"] : "";

        if (name.empty() || room_id == 0) {
            res.status = 400;
            res.set_content("Missing required fields (name, room_id).", "text/plain");
            return;
        }

        auto r = db.bookRoom(name, room_id, check_in, check_out);
        if (r.ok) {
            res.set_content(r.message, "text/plain");
        } else {
            res.status = 400;
            res.set_content(r.message, "text/plain");
        }
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    // POST /cancel
    svr.Post("/cancel", [&](const httplib::Request& req, httplib::Response &res){
        std::string body = req.body;
        auto form = parse_form_urlencoded(body);
        if (!form.count("booking_id")) {
            res.status = 400;
            res.set_content("Missing booking_id", "text/plain");
            return;
        }
        int booking_id = std::stoi(form["booking_id"]);
        auto r = db.cancelBooking(booking_id);
        if (r.ok) res.set_content(r.message, "text/plain");
        else { res.status = 400; res.set_content(r.message, "text/plain"); }
        res.set_header("Access-Control-Allow-Origin", "*");
    });

    std::cout << "Server started at http://localhost:18080\n";
    svr.listen("0.0.0.0", 18080);
    return 0;
}
