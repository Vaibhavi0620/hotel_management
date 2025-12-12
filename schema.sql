PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS rooms (
    room_id INTEGER PRIMARY KEY,
    type TEXT NOT NULL,
    price INTEGER NOT NULL,
    is_available INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS bookings (
    booking_id INTEGER PRIMARY KEY AUTOINCREMENT,
    customer_name TEXT NOT NULL,
    phone TEXT,
    room_id INTEGER NOT NULL,
    check_in TEXT,
    check_out TEXT,
    status TEXT NOT NULL DEFAULT 'active',
    created_at TEXT DEFAULT (datetime('now')),
    FOREIGN KEY(room_id) REFERENCES rooms(room_id)
);

INSERT INTO rooms (room_id, type, price, is_available) VALUES (101, 'Single', 1000, 1);
INSERT INTO rooms (room_id, type, price, is_available) VALUES (102, 'Single', 1000, 1);
INSERT INTO rooms (room_id, type, price, is_available) VALUES (201, 'Double', 2000, 1);
INSERT INTO rooms (room_id, type, price, is_available) VALUES (301, 'Suite', 5000, 1);
INSERT INTO rooms (room_id, type, price, is_available) VALUES (302, 'Suite', 5000, 1);