#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <map>

struct Room {
    int room_id;
    std::string type;
    int price;
    int is_available; // 1 or 0
};

struct BookingResult {
    bool ok;
    std::string message;
    int booking_id; // if any
};

class Database {
public:
    Database() = default;
    ~Database();

    // open DB file (hotel.db) and optionally initialize from SQL file
    bool open(const std::string &dbfile, const std::string &sql_init_file = "");

    // get list of rooms
    std::vector<Room> getRooms();

    // book a room, returns success/failure and booking id
    BookingResult bookRoom(const std::string &name, int room_id,
                           const std::string &check_in, const std::string &check_out);

    // cancel booking by id (sets booking.status = 'cancelled' and set room available)
    BookingResult cancelBooking(int booking_id);

private:
    void close();
    bool execSqlFile(const std::string &sqlfile);

    // sqlite3 handle
    struct sqlite3 *db = nullptr;
};

#endif // DATABASE_H
