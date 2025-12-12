// script.js - Single JS for all pages
let rooms = [
    { id: 101, type: "Deluxe Single", status: "available", price: 8000 },
    { id: 102, type: "Standard Double", status: "available", price: 6000 },
    { id: 103, type: "Deluxe Suite", status: "booked", price: 14000 },
    { id: 104, type: "Family Room", status: "available", price: 9500 },
    { id: 105, type: "Executive Suite", status: "available", price: 18000 },
    { id: 106, type: "Standard Single", status: "booked", price: 5000 }
];

let bookings = [
    { id: 1001, roomId: 101, customer: "Rahul Sharma", checkIn: "2025-12-15", checkOut: "2025-12-18", status: "active" },
    { id: 1002, roomId: 102, customer: "Priya Patel", checkIn: "2025-12-20", checkOut: "2025-12-23", status: "active" },
    { id: 1003, roomId: 104, customer: "Amit Kumar", checkIn: "2025-12-16", checkOut: "2025-12-19", status: "active" },
    { id: 1004, roomId: 103, customer: "Neha Gupta", checkIn: "2025-12-18", checkOut: "2025-12-21", status: "cancelled" }
];

function initData() {
    const savedRooms = localStorage.getItem('hotelRooms');
    const savedBookings = localStorage.getItem('hotelBookings');
    if (savedRooms) rooms = JSON.parse(savedRooms);
    if (savedBookings) bookings = JSON.parse(savedBookings);
}

function saveData() {
    localStorage.setItem('hotelRooms', JSON.stringify(rooms));
    localStorage.setItem('hotelBookings', JSON.stringify(bookings));
}

function formatIndianDate(dateStr) {
    const date = new Date(dateStr + 'T00:00:00');
    return date.toLocaleDateString('en-GB', { day: '2-digit', month: '2-digit', year: '2-digit' });
}

function formatRupees(amount) {
    return new Intl.NumberFormat('en-IN', { style: 'currency', currency: 'INR' }).format(amount);
}

function initPage(pageType) {
    initData();
    
    if (pageType === 'home') initHome();
    else if (pageType === 'book') initBook();
    else if (pageType === 'cancel') initCancel();
    else if (pageType === 'success') initSuccess();
    
    window.addEventListener('beforeunload', saveData);
}

function initHome() {
    updateDashboard();
    showRoomStatus();
}

function updateDashboard() {
    const available = rooms.filter(r => r.status === 'available').length;
    const booked = rooms.length - available;
    const active = bookings.filter(b => b.status === 'active').length;
    
    document.getElementById('availableRooms').textContent = available;
    document.getElementById('bookedRooms').textContent = booked;
    document.getElementById('activeBookings').textContent = active;
}

function showRoomStatus() {
    const statusDiv = document.getElementById('quickStatus');
    if (!statusDiv) return;
    
    statusDiv.innerHTML = `
        <div class="status-item">
            <div class="status-circle ${rooms.find(r => r.id === 101)?.status || 'available'}">101<br>Deluxe</div>
            <p>${rooms.find(r => r.id === 101)?.status === 'available' ? '✅ Available' : '❌ Booked'}</p>
        </div>
        <div class="status-item">
            <div class="status-circle ${rooms.find(r => r.id === 102)?.status || 'available'}">102</div>
            <p>${rooms.find(r => r.id === 102)?.status === 'available' ? '✅ Available' : '❌ Booked'}</p>
        </div>
        <div class="status-item">
            <div class="status-circle ${rooms.find(r => r.id === 105)?.status || 'available'}">105<br>Suite</div>
            <p>${rooms.find(r => r.id === 105)?.status === 'available' ? '✅ Available' : '❌ Booked'}</p>
        </div>
    `;
}

function initBook() {
    displayRooms();
    setupBookForm();
}

function displayRooms() {
    const roomList = document.getElementById('roomList');
    roomList.innerHTML = rooms.map(room => `
        <div class="room-card">
            <h4>Room ${room.id}</h4>
            <p><strong>${room.type}</strong></p>
            <p>${formatRupees(room.price)}/night</p>
            <div class="status ${room.status}">
                ${room.status === 'available' ? '✅ Available' : '❌ Booked'}
            </div>
        </div>
    `).join('');
}

function setupBookForm() {
    const roomInput = document.getElementById('roomInput');
    roomInput.addEventListener('input', function() {
        const roomId = parseInt(this.value);
        const room = rooms.find(r => r.id === roomId);
        const statusDiv = document.getElementById('bookingStatus');
        const bookBtn = document.getElementById('bookBtn');
        
        if (room) {
            statusDiv.innerHTML = `
                <div class="success">
                    ${room.type} - ${room.status === 'available' ? '✅ Available' : '❌ Booked'}<br>
                    ${formatRupees(room.price)}/night
                </div>
            `;
            statusDiv.style.display = 'block';
            bookBtn.disabled = room.status !== 'available';
            bookBtn.textContent = room.status === 'available' ? 'Confirm Booking' : 'Room Unavailable';
        } else {
            statusDiv.innerHTML = '<div class="error">Room not found</div>';
            statusDiv.style.display = 'block';
            bookBtn.disabled = true;
        }
    });

    document.getElementById('bookForm').addEventListener('submit', function(e) {
        e.preventDefault();
        const formData = new FormData(this);
        const roomId = parseInt(formData.get('room_id'));
        const room = rooms.find(r => r.id === roomId);
        
        if (room && room.status === 'available') {
            const newBooking = {
                id: Math.max(...bookings.map(b => b.id)) + 1,
                roomId: roomId,
                customer: formData.get('name'),
                checkIn: formData.get('check_in'),
                checkOut: formData.get('check_out'),
                status: 'active'
            };
            bookings.push(newBooking);
            room.status = 'booked';
            
            displayRooms();
            alert(`✅ Booking #${newBooking.id} confirmed!\n\n${newBooking.customer}\nRoom ${roomId}\n${formatIndianDate(newBooking.checkIn)} - ${formatIndianDate(newBooking.checkOut)}`);
            
            this.reset();
            document.getElementById('bookingStatus').style.display = 'none';
            window.location.href = `success.html?action=book&booking=${newBooking.id}`;
        } else {
            alert('❌ Room not available');
        }
    });
}

function initCancel() {
    displayBookings();
    setupCancelForm();
}

function displayBookings() {
    const bookingList = document.getElementById('bookingList');
    bookingList.innerHTML = bookings.map(booking => `
        <div class="booking-card">
            <h4>Booking #${booking.id}</h4>
            <p><strong>Room ${booking.roomId}</strong></p>
            <p>Guest: ${booking.customer}</p>
            <p>📅 ${formatIndianDate(booking.checkIn)} - ${formatIndianDate(booking.checkOut)}</p>
            <div class="status ${booking.status}">
                ${booking.status === 'active' ? '✅ Active' : '❌ Cancelled'}
            </div>
        </div>
    `).join('');
}

function setupCancelForm() {
    const bookingInput = document.getElementById('bookingInput');
    bookingInput.addEventListener('input', function() {
        const bookingId = parseInt(this.value);
        const booking = bookings.find(b => b.id === bookingId);
        const statusDiv = document.getElementById('cancelStatus');
        const cancelBtn = document.getElementById('cancelBtn');
        
        if (booking && booking.status === 'active') {
            const room = rooms.find(r => r.id === booking.roomId);
            statusDiv.innerHTML = `
                <div class="success">
                    ${booking.customer}<br>
                    Room ${booking.roomId} (${room?.type || ''})<br>
                    ${formatIndianDate(booking.checkIn)} - ${formatIndianDate(booking.checkOut)}
                </div>
            `;
            statusDiv.style.display = 'block';
            cancelBtn.disabled = false;
        } else if (booking) {
            statusDiv.innerHTML = '<div class="error">Already cancelled</div>';
            statusDiv.style.display = 'block';
            cancelBtn.disabled = true;
        } else {
            statusDiv.innerHTML = '<div class="error">Booking not found</div>';
            statusDiv.style.display = 'block';
            cancelBtn.disabled = true;
        }
    });

    document.getElementById('cancelForm').addEventListener('submit', function(e) {
        e.preventDefault();
        const bookingId = parseInt(document.querySelector('input[name="booking_id"]').value);
        const booking = bookings.find(b => b.id === bookingId);
        
        if (booking && booking.status === 'active') {
            booking.status = 'cancelled';
            const room = rooms.find(r => r.id === booking.roomId);
            if (room) room.status = 'available';
            
            displayBookings();
            alert(`✅ Booking #${bookingId} cancelled successfully!`);
            this.reset();
            document.getElementById('cancelStatus').style.display = 'none';
            window.location.href = 'success.html?action=cancel';
        }
    });
}

function initSuccess() {
    const urlParams = new URLSearchParams(window.location.search);
    const action = urlParams.get('action');
    const bookingId = urlParams.get('booking');
    
    if (action === 'book' && bookingId) {
        const booking = bookings.find(b => b.id == bookingId);
        if (booking) {
            const room = rooms.find(r => r.id === booking.roomId);
            document.getElementById('successMessage').innerHTML = `
                <div class="booking-details">
                    <h3>✅ Booking Confirmed!</h3>
                    <div class="detail-row"><span>Booking ID:</span> <strong>#${booking.id}</strong></div>
                    <div class="detail-row"><span>Room:</span> <strong>${room?.type}</strong></div>
                    <div class="detail-row"><span>Guest:</span> <strong>${booking.customer}</strong></div>
                    <div class="detail-row"><span>Dates:</span> <strong>${formatIndianDate(booking.checkIn)} - ${formatIndianDate(booking.checkOut)}</strong></div>
                    <div class="detail-row"><span>Rate:</span> <strong>${formatRupees(room?.price || 0)}/night</strong></div>
                </div>
            `;
        }
    } else if (action === 'cancel') {
        document.getElementById('successMessage').innerHTML = '<div class="booking-details"><h3>✅ Cancellation Confirmed!</h3><p>Room is now available for booking.</p></div>';
    }
    
    updateSuccessStats();
}

function updateSuccessStats() {
    const available = rooms.filter(r => r.status === 'available').length;
    const booked = rooms.length - available;
    const active = bookings.filter(b => b.status === 'active').length;
    
    document.getElementById('hotelStats').innerHTML = `
        <div class="stat"><div style="font-size:2rem;color:#28a745;">${available}</div>Available</div>
        <div class="stat"><div style="font-size:2rem;color:#dc3545;">${booked}</div>Booked</div>
        <div class="stat"><div style="font-size:2rem;color:#007bff;">${active}</div>Active</div>
    `;
}

// Modal functions for home page
function showQuickSearch() {
    document.getElementById('searchModal').style.display = 'flex';
}

function closeQuickSearch() {
    document.getElementById('searchModal').style.display = 'none';
    document.getElementById('searchResult').style.display = 'none';
}

function performQuickSearch() {
    const query = parseInt(document.getElementById('quickSearchInput').value);
    const resultDiv = document.getElementById('searchResult');
    const foundBooking = bookings.find(b => b.id === query || b.roomId === query);
    const foundRoom = rooms.find(r => r.id === query);
    
    if (foundBooking) {
        resultDiv.innerHTML = `<div style="background:#d4edda;color:#155724;padding:1rem;border-radius:8px;">
            <strong>Booking #${foundBooking.id}</strong><br>Room ${foundBooking.roomId}<br>${foundBooking.customer}<br>
            ${foundBooking.status === 'active' ? '✅ Active' : '❌ Cancelled'}
        </div>`;
    } else if (foundRoom) {
        resultDiv.innerHTML = `<div style="background:#e2e3e5;color:#383d41;padding:1rem;border-radius:8px;">
            <strong>Room ${foundRoom.id}</strong><br>${foundRoom.type}<br>
            ${foundRoom.status === 'available' ? '✅ Available' : '❌ Booked'}
        </div>`;
    } else {
        resultDiv.innerHTML = '<div style="background:#f8d7da;color:#721c24;padding:1rem;border-radius:8px;">Not found</div>';
    }
    resultDiv.style.display = 'block';
}
