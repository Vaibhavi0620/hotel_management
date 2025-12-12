#include "database.h"
#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <iostream>

Database::~Database() {
    close();
}

void Database::close() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool Database::open(const std::string &dbfile, const std::string &sql_init_file) {
    if (sqlite3_open(dbfile.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Cannot open DB: " << sqlite3_errmsg(db) << std::endl;
        close();
        return false;
    }
    // enable foreign keys
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    if (!sql_init_file.empty()) {
        if (!execSqlFile(sql_init_file)) {
            std::cerr << "Failed to run init SQL file\n";
            return false;
        }
    }
    return true;
}

bool Database::execSqlFile(const std::string &sqlfile) {
    std::ifstream in(sqlfile);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string sql = ss.str();

    char *err = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << (err ? err : "unknown") << std::endl;
        if (err) sqlite3_free(err);
        return false;
    }
    return true;
}

std::vector<Room> Database::getRooms() {
    std::vector<Room> out;
    const char *q = "SELECT room_id, type, price, is_available FROM rooms ORDER BY room_id;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, q, -1, &stmt, nullptr) != SQLITE_OK) return out;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Room r;
        r.room_id = sqlite3_column_int(stmt, 0);
        const unsigned char *t = sqlite3_column_text(stmt, 1);
        r.type = t ? reinterpret_cast<const char*>(t) : "";
        r.price = sqlite3_column_int(stmt, 2);
        r.is_available = sqlite3_column_int(stmt, 3);
        out.push_back(r);
    }
    sqlite3_finalize(stmt);
    return out;
}

BookingResult Database::bookRoom(const std::string &name, int room_id,
                                 const std::string &check_in, const std::string &check_out) {
    BookingResult res{false, "Unknown error", -1};

    // Check availability
    const char *chk_sql = "SELECT is_available FROM rooms WHERE room_id = ?;";
    sqlite3_stmt *chk_stmt = nullptr;
    if (sqlite3_prepare_v2(db, chk_sql, -1, &chk_stmt, nullptr) != SQLITE_OK) {
        res.message = "DB prepare error (check)";
        return res;
    }
    sqlite3_bind_int(chk_stmt, 1, room_id);
    int rc = sqlite3_step(chk_stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(chk_stmt);
        res.message = "Room not found";
        return res;
    }
    int is_avail = sqlite3_column_int(chk_stmt, 0);
    sqlite3_finalize(chk_stmt);

    if (!is_avail) {
        res.message = "Room not available";
        return res;
    }

    // Begin transaction
    char *err = nullptr;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &err);

    // Insert booking
    const char *ins_sql = "INSERT INTO bookings (customer_name, room_id, check_in, check_out, status) VALUES (?, ?, ?, ?, 'active');";
    sqlite3_stmt *ins_stmt = nullptr;
    if (sqlite3_prepare_v2(db, ins_sql, -1, &ins_stmt, nullptr) != SQLITE_OK) {
        res.message = "DB prepare error (insert)";
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return res;
    }
    sqlite3_bind_text(ins_stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(ins_stmt, 2, room_id);
    sqlite3_bind_text(ins_stmt, 3, check_in.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(ins_stmt, 4, check_out.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(ins_stmt) != SQLITE_DONE) {
        res.message = "Failed to insert booking";
        sqlite3_finalize(ins_stmt);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return res;
    }
    sqlite3_finalize(ins_stmt);

    // get last inserted id
    int booking_id = (int) sqlite3_last_insert_rowid(db);

    // mark room unavailable
    const char *upd_sql = "UPDATE rooms SET is_available = 0 WHERE room_id = ?;";
    sqlite3_stmt *upd_stmt = nullptr;
    if (sqlite3_prepare_v2(db, upd_sql, -1, &upd_stmt, nullptr) != SQLITE_OK) {
        res.message = "DB prepare error (update)";
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return res;
    }
    sqlite3_bind_int(upd_stmt, 1, room_id);
    if (sqlite3_step(upd_stmt) != SQLITE_DONE) {
        sqlite3_finalize(upd_stmt);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        res.message = "Failed to mark room booked";
        return res;
    }
    sqlite3_finalize(upd_stmt);

    // commit
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err);

    res.ok = true;
    res.booking_id = booking_id;
    res.message = "Booked successfully. Booking ID: " + std::to_string(booking_id);
    return res;
}

BookingResult Database::cancelBooking(int booking_id) {
    BookingResult res{false, "Unknown error", booking_id};

    // find booking
    const char *q = "SELECT room_id, status FROM bookings WHERE booking_id = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, q, -1, &stmt, nullptr) != SQLITE_OK) {
        res.message = "DB prepare error (find)";
        return res;
    }
    sqlite3_bind_int(stmt, 1, booking_id);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        res.message = "Booking not found";
        return res;
    }
    int room_id = sqlite3_column_int(stmt, 0);
    const unsigned char *st = sqlite3_column_text(stmt, 1);
    std::string status = st ? reinterpret_cast<const char*>(st) : "";
    sqlite3_finalize(stmt);

    if (status == "cancelled") {
        res.message = "Booking already cancelled";
        return res;
    }

    // Begin transaction
    char *err = nullptr;
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &err);

    // update booking status
    const char *upd_book = "UPDATE bookings SET status = 'cancelled' WHERE booking_id = ?;";
    sqlite3_stmt *upd_stmt = nullptr;
    if (sqlite3_prepare_v2(db, upd_book, -1, &upd_stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        res.message = "DB prepare error (update booking)";
        return res;
    }
    sqlite3_bind_int(upd_stmt, 1, booking_id);
    if (sqlite3_step(upd_stmt) != SQLITE_DONE) {
        sqlite3_finalize(upd_stmt);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        res.message = "Failed to update booking";
        return res;
    }
    sqlite3_finalize(upd_stmt);

    // set room available
    const char *upd_room = "UPDATE rooms SET is_available = 1 WHERE room_id = ?;";
    sqlite3_stmt *up2 = nullptr;
    if (sqlite3_prepare_v2(db, upd_room, -1, &up2, nullptr) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        res.message = "DB prepare error (update room)";
        return res;
    }
    sqlite3_bind_int(up2, 1, room_id);
    if (sqlite3_step(up2) != SQLITE_DONE) {
        sqlite3_finalize(up2);
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        res.message = "Failed to mark room available";
        return res;
    }
    sqlite3_finalize(up2);

    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err);
    res.ok = true;
    res.message = "Booking cancelled and room marked available";
    return res;
}
