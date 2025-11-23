#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

// --- AP Configuration ---
const char *ssid = "ESP_AP_Server";  // SSID of the Access Point
const char *password = "12345678";   // Password for the AP (must be at least 8 characters)
String slotData[5];                  // Array to store slot information (1-based index, supports 1-4)

// The server object will handle HTTP requests on port 80
ESP8266WebServer server(80);

// Variable to store the latest message received from the client
String currentMessage = "Waiting for data from client...";

// --- FORWARD DECLARATIONS (Required for C++ compilation order) ---
void handleRoot();
void handleDataFetch();
void handleDataSend();

// --- Ultrasonic + Servo Gate Setup ---
#define TRIG D6  // Trigger pin for ultrasonic sensor
#define ECHO D5  // Echo pin for ultrasonic sensor
#define BUZZER D1 // Buzzer pin for indicating when the garage is full
#define SERVO D4 //Gate Servo


Servo gateServo;  // Servo to control the gate

unsigned long gateOpenTime = 0;  // To store the time when the gate opened
bool gateIsOpen = false;         // Flag to check if the gate is open

// Function to read distance from ultrasonic sensor
long readDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 30000);  // Read the duration of the pulse
  long distance = duration * 0.034 / 2;       // Calculate the distance in cm
  return distance;
}

// Function to open the gate (move the servo to 90 degrees)
void openGate() {
  gateServo.write(130);  // Adjust this value if necessary
}

// Function to close the gate (move the servo to 0 degrees)
void closeGate() {
  gateServo.write(0);   // Adjust this value if necessary
}

// Function to check if there are free slots in the parking system
bool hasFreeSlots() {
  for (int i = 1; i <= 4; i++) {
    // If a slot is empty (data is not sent), it is considered free
    if (slotData[i].length() == 0) {
      return true;
    }

    int statIndex = slotData[i].indexOf("stat:");
    if (statIndex == -1) continue;  // If the status is not found, skip

    int status = slotData[i].substring(statIndex + 5).toInt();

    if (status == 0) return true;  // If the slot status is 0 (free), return true
  }
  return false;  // If no free slots found, return false
}

// --- Function to activate buzzer for 3 beeps ---
void beepBuzzer() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER, 2000);      // Turn the buzzer on
    delay(200);                     // Wait for 200ms
    noTone(BUZZER);                // Turn the buzzer off
    delay(200);                   // Wait for 200ms before next beep
  }
}

void setup() {
  Serial.begin(115200);  // Start the serial monitor at 115200 baud
  delay(100);

  // Set up the device as an Access Point (AP)
  Serial.print("Setting up Access Point...");
  WiFi.softAP(ssid, password);  // Set the SSID and password for the AP

  IPAddress myIP = WiFi.softAPIP();  // Get the IP address of the AP
  Serial.print("AP IP address: ");
  Serial.println(myIP);  // Print the AP's IP address

  // Define the server routes (URLs) and associate them with handler functions
  server.on("/", handleRoot);  // Main page request
  server.on("/data", handleDataFetch);  // Request to fetch data
  server.on("/send", handleDataSend);  // Request to send data

  // Start the web server
  server.begin();
  Serial.println("HTTP Server started.");

  // Set up ultrasonic sensor pins
  pinMode(TRIG, OUTPUT);  // Set the trigger pin as output
  pinMode(ECHO, INPUT);   // Set the echo pin as input

  // Set up the servo
  gateServo.attach(SERVO);  // Attach the servo to the D4 pin (can use any available pin)
  closeGate();            // Make sure the gate is closed initially

  //Setup the buzzer
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW); // ensure it's off

}

void loop() {
  server.handleClient();  // Handle incoming HTTP requests

  long distance = readDistance();  // Read distance from ultrasonic sensor
  bool freeSlotExists = hasFreeSlots();  // Check if there are any free parking slots

  // --- CAR DETECTED + FREE SLOTS → OPEN GATE ---
  if (distance > 0 && distance < 3 && freeSlotExists) {
    if (!gateIsOpen) {  // Open the gate only once
      openGate();
      gateIsOpen = true;
      gateOpenTime = millis();  // Start a timer to close the gate after 5 seconds
    }
  }

  // --- PARKING FULL → CLOSE GATE IMMEDIATELY ---
  else if (distance > 0 && distance < 3 && !freeSlotExists) {
    closeGate();  // Close the gate if the parking is full
    gateIsOpen = false;
    beepBuzzer();  // Activate the buzzer with 3 beeps
  }

  // --- CHECK IF 5 SECONDS PASSED AND CLOSE GATE ---
  if (gateIsOpen && (millis() - gateOpenTime >= 5000)) {
    closeGate();  // Close the gate after 5 seconds
    gateIsOpen = false;
  }
}

// --- Handler Functions ---

// Handles the request for the main webpage
void handleRoot() {
  Serial.println("Serving main webpage to client.");

  // Mobile-friendly HTML (this part should be replaced with your actual HTML content)
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Parking System</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: Arial, sans-serif;
            background: #f3f4f6;
            padding: 12px;
            min-height: 100vh;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        
        /* Header */
        .header { text-align: center; margin-bottom: 20px; }
        .header h1 { 
            font-size: 24px; 
            color: #1f2937; 
            margin-bottom: 8px;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
        }
        .header p { color: #6b7280; font-size: 14px; }
        
        /* Stats Grid */
        .stats-grid { 
            display: grid; 
            grid-template-columns: repeat(3, 1fr); 
            gap: 12px; 
            margin-bottom: 20px; 
        }
        .stat-card {
            background: white;
            border-radius: 8px;
            padding: 16px;
            text-align: center;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }
        .stat-value { 
            font-size: 20px; 
            font-weight: bold; 
            margin-bottom: 4px; 
        }
        .available { color: #059669; }
        .occupied { color: #dc2626; }
        .revenue { color: #2563eb; }
        .stat-label { color: #6b7280; font-size: 12px; }
        
        /* Gate Status */
        .gate-card {
            background: white;
            border-radius: 8px;
            padding: 16px;
            text-align: center;
            margin-bottom: 20px;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }
        .gate-title { 
            font-size: 18px; 
            color: #1f2937; 
            margin-bottom: 12px; 
            font-weight: bold;
        }
        .gate-status {
            display: inline-flex;
            align-items: center;
            gap: 8px;
            padding: 12px 20px;
            border-radius: 8px;
            font-weight: bold;
        }
        .gate-open { 
            background: #dcfce7; 
            border: 1px solid #bbf7d0;
            color: #166534;
        }
        .gate-closed { 
            background: #fef2f2; 
            border: 1px solid #fecaca;
            color: #dc2626;
        }
        
        /* Parking Slots */
        .slots-card {
            background: white;
            border-radius: 8px;
            padding: 16px;
            margin-bottom: 16px;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }
        .slots-title { 
            font-size: 18px; 
            color: #1f2937; 
            margin-bottom: 16px; 
            text-align: center;
            font-weight: bold;
        }
        
        /* Desktop Table */
        .desktop-table { width: 100%; border-collapse: collapse; }
        .desktop-table th {
            background: #f8fafc;
            padding: 12px;
            text-align: left;
            font-size: 12px;
            color: #374151;
            font-weight: 600;
            text-transform: uppercase;
            border-bottom: 1px solid #e5e7eb;
        }
        .desktop-table td {
            padding: 12px;
            border-bottom: 1px solid #e5e7eb;
        }
        .desktop-table tr:hover { background: #f9fafb; }
        
        /* Status Badge */
        .status-badge {
            padding: 6px 12px;
            border-radius: 20px;
            font-size: 12px;
            font-weight: 500;
        }
        .status-available { background: #dcfce7; color: #166534; }
        .status-occupied { background: #fef2f2; color: #dc2626; }
        
        /* Slot Rectangle */
        .slot-rectangle {
            width: 80px;
            height: 40px;
            border-radius: 6px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-weight: bold;
            color: white;
            font-size: 12px;
        }
        .slot-free { background: linear-gradient(135deg, #10b981, #059669); }
        .slot-occupied { background: linear-gradient(135deg, #ef4444, #dc2626); }
        
        /* Mobile Cards */
        .mobile-slots { display: none; }
        .mobile-slot-card {
            background: white;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            padding: 12px;
            margin-bottom: 8px;
        }
        .mobile-slot-header {
            display: flex;
            justify-content: between;
            align-items: center;
            margin-bottom: 8px;
        }
        .mobile-slot-title { 
            font-size: 16px; 
            font-weight: bold; 
            color: #1f2937; 
        }
        .mobile-slot-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 8px;
        }
        .mobile-slot-item {
            background: #f8fafc;
            border-radius: 6px;
            padding: 8px;
            text-align: center;
        }
        .mobile-label { 
            color: #6b7280; 
            font-size: 10px; 
            margin-bottom: 2px; 
        }
        .mobile-value { 
            font-size: 12px; 
            font-weight: 600; 
        }
        
        /* System Info */
        .system-info {
            background: white;
            border-radius: 8px;
            padding: 12px;
            text-align: center;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }
        .current-bills { 
            font-weight: 600; 
            color: #059669; 
            font-size: 14px;
            margin-bottom: 4px;
        }
        .last-update { 
            color: #6b7280; 
            font-size: 12px; 
        }
        
        /* Popup */
        .popup-overlay {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0,0,0,0.5);
            display: none;
            align-items: center;
            justify-content: center;
            z-index: 1000;
            padding: 20px;
        }
        .popup-overlay.show { display: flex; }
        .popup-content {
            background: white;
            border-radius: 12px;
            max-width: 320px;
            width: 100%;
            overflow: hidden;
        }
        .popup-header {
            background: linear-gradient(135deg, #10b981, #059669);
            padding: 20px;
            text-align: center;
            color: white;
        }
        .popup-icon {
            width: 48px;
            height: 48px;
            background: rgba(255,255,255,0.2);
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            margin: 0 auto 12px;
        }
        .popup-title {
            font-size: 20px;
            font-weight: bold;
            margin-bottom: 4px;
        }
        .popup-subtitle { font-size: 14px; opacity: 0.9; }
        .popup-body { padding: 20px; }
        .popup-bill-item {
            background: #f8fafc;
            border-radius: 8px;
            padding: 16px;
            border: 1px solid #e5e7eb;
            margin-bottom: 12px;
        }
        .popup-slot { 
            font-size: 16px; 
            font-weight: bold; 
            color: #1f2937;
            margin-bottom: 4px;
        }
        .popup-amount { 
            font-size: 24px; 
            font-weight: bold; 
            color: #059669;
            margin-bottom: 4px;
        }
        .popup-thankyou { 
            color: #6b7280; 
            font-size: 12px; 
        }
        .popup-button {
            width: 100%;
            background: linear-gradient(135deg, #10b981, #059669);
            color: white;
            border: none;
            padding: 12px;
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
        }
        .popup-button:hover { opacity: 0.9; }
        
        /* Animations */
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.7; }
        }
        .pulse { animation: pulse 2s infinite; }
        
        /* Responsive */
        @media (max-width: 768px) {
            .desktop-table { display: none; }
            .mobile-slots { display: block; }
            .stats-grid { grid-template-columns: repeat(3, 1fr); }
            .header h1 { font-size: 20px; }
        }
        
        @media (max-width: 480px) {
            .stats-grid { grid-template-columns: 1fr; gap: 8px; }
            body { padding: 8px; }
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <div class="header">
            <h1>Smart Parking System</h1>
            <p>Real-time monitoring</p>
        </div>

        <!-- Quick Stats -->
        <div class="stats-grid">
            <div class="stat-card">
                <div class="stat-value available" id="available-slots">4</div>
                <div class="stat-label">Available</div>
            </div>
            <div class="stat-card">
                <div class="stat-value occupied" id="occupied-slots">0</div>
                <div class="stat-label">Occupied</div>
            </div>
            <div class="stat-card">
                <div class="stat-value revenue" id="total-revenue">$0.00</div>
                <div class="stat-label">Revenue</div>
            </div>
        </div>

        <!-- Gate Status -->
        <div class="gate-card">
            <div class="gate-title">Gate Status</div>
            <div id="gate-status" class="gate-status gate-closed">
                <span>❌</span>
                <span>Closed</span>
            </div>
        </div>

        <!-- Parking Slots -->
        <div class="slots-card">
            <div class="slots-title">Parking Slots</div>
            
            <!-- Desktop Table -->
            <table class="desktop-table">
                <thead>
                    <tr>
                        <th>Slot</th>
                        <th>Status</th>
                        <th>Duration</th>
                        <th>Bill</th>
                        <th>Status</th>
                    </tr>
                </thead>
                <tbody id="slot-table-body-desktop">
                    <!-- Desktop rows populated by JS -->
                </tbody>
            </table>

            <!-- Mobile Cards -->
            <div class="mobile-slots" id="slot-table-body-mobile">
                <!-- Mobile cards populated by JS -->
            </div>
        </div>

        <!-- System Info -->
        <div class="system-info">
            <div class="current-bills" id="current-bills">Current Bills: Loading...</div>
            <div class="last-update" id="last-update">Last updated: Just now</div>
        </div>
    </div>

    <!-- Popup Modal -->
    <div class="popup-overlay" id="popup">
        <div class="popup-content">
            <div class="popup-header">
                <div class="popup-icon">🧾</div>
                <div class="popup-title">Payment Receipt</div>
                <div class="popup-subtitle">Slot released</div>
            </div>
            <div class="popup-body">
                <div id="popup-text">
                    <!-- Bill details populated by JS -->
                </div>
                <button class="popup-button" onclick="closePopup()">CONFIRM PAYMENT</button>
            </div>
        </div>
    </div>

    <script>
        // Global variables
        let previousSlotData = {};
        let billList = [];

        // Popup functions
        function showPopup(text) {
            billList.push(text);
            
            let html = billList.map(line => {
                let slotMatch = line.match(/Slot (\d+).*Bill: \$([\d.]+)/);
                if (slotMatch) {
                    return `
                    <div class="popup-bill-item">
                        <div class="popup-slot">Slot ${slotMatch[1]}</div>
                        <div class="popup-amount">$${slotMatch[2]}</div>
                        <div class="popup-thankyou">Thank you for your payment!</div>
                    </div>`;
                }
                return `<div style="text-align: center; color: #374151; margin-bottom: 12px;">${line}</div>`;
            }).join("");
            
            document.getElementById("popup-text").innerHTML = html;
            document.getElementById("popup").classList.add("show");
        }

        function closePopup() {
            document.getElementById("popup").classList.remove("show");
            setTimeout(() => {
                billList = [];
                document.getElementById("popup-text").innerHTML = "";
            }, 300);
        }

        // Data parsing
        function parseSlots(raw) {
            let lines = raw.split("\n");
            let result = {};
            lines.forEach(line => {
                let match = line.match(/slot:\s*(\d+),\s*stat:\s*(\d+),\s*time:\s*([\d.]+),\s*bill:\s*([\d.]+)/);
                if (match) {
                    let id = match[1];
                    result[id] = {
                        status: Number(match[2]),
                        time: Number(match[3]),
                        bill: Number(match[4])
                    };
                }
            });
            return result;
        }

        // Slot monitoring
        function checkForReleasedSlots(current) {
            for (let id in current) {
                let prev = previousSlotData[id];
                let now = current[id];
                if (prev && prev.status === 1 && now.status === 0) {
                    showPopup(`Slot ${id} has been released\nTotal Bill: $${prev.bill.toFixed(2)}`);
                }
            }
            previousSlotData = current;
        }

        // Gate status
        function updateGateStatus(hasFreeSlots) {
            const gateElement = document.getElementById('gate-status');
            if (hasFreeSlots) {
                gateElement.innerHTML = '<span>Unlocked</span>';
                gateElement.className = 'gate-status gate-open pulse';
            } else {
                gateElement.innerHTML = '<span>Locked</span>';
                gateElement.className = 'gate-status gate-closed';
            }
        }

        // Statistics
        function updateStats(currentSlots) {
            let available = 0;
            let occupied = 0;
            let totalRevenue = 0;

            for (let id = 1; id <= 4; id++) {
                let slot = currentSlots[id];
                if (slot) {
                    if (slot.status === 1) {
                        occupied++;
                        totalRevenue += slot.bill;
                    } else {
                        available++;
                    }
                } else {
                    available++;
                }
            }

            document.getElementById('available-slots').textContent = available;
            document.getElementById('occupied-slots').textContent = occupied;
            document.getElementById('total-revenue').textContent = '$' + totalRevenue.toFixed(2);
            
            updateGateStatus(available > 0);
        }

        // Time formatting
        function formatTime(seconds) {
            if (seconds < 60) return seconds + 's';
            let minutes = Math.floor(seconds / 60);
            let remainingSeconds = seconds % 60;
            if (minutes < 60) return minutes + 'm ' + remainingSeconds + 's';
            let hours = Math.floor(minutes / 60);
            let remainingMinutes = minutes % 60;
            return hours + 'h ' + remainingMinutes + 'm';
        }

        // Current bills display
        function updateCurrentBills(currentSlots) {
            let bills = [];
            for (let id = 1; id <= 4; id++) {
                let slot = currentSlots[id];
                if (slot && slot.status === 1) {
                    bills.push(`Slot ${id}: $${slot.bill.toFixed(2)}`);
                }
            }
            
            if (bills.length > 0) {
                document.getElementById('current-bills').textContent = 'Current Bills: ' + bills.join(' | ');
            } else {
                document.getElementById('current-bills').textContent = 'Current Bills: No active bills';
            }
        }

        // Slot table rendering
        function updateSlotTable(currentSlots) {
            // Desktop Table
            let desktopTbody = document.getElementById('slot-table-body-desktop');
            let desktopHtml = '';

            // Mobile Cards
            let mobileContainer = document.getElementById('slot-table-body-mobile');
            let mobileHtml = '';

            for (let id = 1; id <= 4; id++) {
                let slot = currentSlots[id];
                let isOccupied = slot ? slot.status === 1 : false;
                let time = slot ? slot.time : 0;
                let bill = slot ? slot.bill.toFixed(2) : "0.00";
                let statusText = isOccupied ? "Occupied" : "Available";
                let statusClass = isOccupied ? "status-occupied" : "status-available";

                // Desktop row
                desktopHtml += `
                <tr>
                    <td><strong>${id}</strong></td>
                    <td><span class="status-badge ${statusClass}">${statusText}</span></td>
                    <td style="font-family: monospace;">${formatTime(time)}</td>
                    <td><strong>$${bill}</strong></td>
                    <td><div class="slot-rectangle ${isOccupied ? 'slot-occupied' : 'slot-free'}">${isOccupied ? 'BUSY' : 'FREE'}</div></td>
                </tr>`;

                // Mobile card
                mobileHtml += `
                <div class="mobile-slot-card">
                    <div class="mobile-slot-header">
                        <div class="mobile-slot-title">Slot ${id}</div>
                        <div class="slot-rectangle ${isOccupied ? 'slot-occupied' : 'slot-free'}">${isOccupied ? 'BUSY' : 'FREE'}</div>
                    </div>
                    <div class="mobile-slot-grid">
                        <div class="mobile-slot-item">
                            <div class="mobile-label">Status</div>
                            <div class="mobile-value" style="color: ${isOccupied ? '#dc2626' : '#059669'}">${statusText}</div>
                        </div>
                        <div class="mobile-slot-item">
                            <div class="mobile-label">Duration</div>
                            <div class="mobile-value">${formatTime(time)}</div>
                        </div>
                        <div class="mobile-slot-item">
                            <div class="mobile-label">Current Bill</div>
                            <div class="mobile-value" style="color: #059669">$${bill}</div>
                        </div>
                        <div class="mobile-slot-item">
                            <div class="mobile-label">Slot ID</div>
                            <div class="mobile-value">#${id}</div>
                        </div>
                    </div>
                </div>`;
            }

            desktopTbody.innerHTML = desktopHtml;
            mobileContainer.innerHTML = mobileHtml;
        }

        // Last update time
        function updateLastUpdateTime() {
            const now = new Date();
            document.getElementById('last-update').textContent = 'Last updated: ' + now.toLocaleTimeString();
        }

        // Data fetching
        function fetchData() {
            fetch('/data')
                .then(response => response.text())
                .then(raw => {
                    let currentSlots = parseSlots(raw);
                    updateSlotTable(currentSlots);
                    updateStats(currentSlots);
                    updateCurrentBills(currentSlots);
                    checkForReleasedSlots(currentSlots);
                    updateLastUpdateTime();
                })
                .catch(err => console.error('Error fetching data:', err));
        }

        // Initialize
        document.addEventListener('DOMContentLoaded', function() {
            fetchData();
            setInterval(fetchData, 2000); // Update every 2 seconds
        });
    </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", html);  // Send the HTML content to the client
}

// Handles the request to fetch the latest parking data
void handleDataFetch() {
  server.send(200, "text/plain", currentMessage);  // Send the current data as plain text
}

// Handles the request to send data (e.g., parking slot updates)
void handleDataSend() {
  if (!server.hasArg("message")) {
    server.send(400, "text/plain", "Error: 'message' parameter missing.");
    return;  // If the 'message' parameter is missing, return an error
  }

  String raw = server.arg("message");  // Get the raw data sent in the message
  raw.replace("%0A", "\n");  // Replace URL encoded newline characters
  raw.replace("%0D", "");    // Replace URL encoded carriage return characters
  Serial.println("RAW REQUEST:");
  Serial.println(raw);  // Print the raw request for debugging

  // Split the data by newlines and process each line
  int start = 0;
  while (true) {
    int end = raw.indexOf('\n', start);
    String line = (end == -1) ? raw.substring(start) : raw.substring(start, end);

    if (line.length() > 2) {
      Serial.print("PROCESSING: ");
      Serial.println(line);

      int p1 = line.indexOf(',');
      int p2 = line.indexOf(',', p1 + 1);
      int p3 = line.indexOf(',', p2 + 1);

      if (p1 != -1 && p2 != -1 && p3 != -1) {
        int slotID = line.substring(0, p1).toInt();
        int status = line.substring(p1 + 1, p2).toInt();
        long occupiedTime = line.substring(p2 + 1, p3).toInt();
        float bill = line.substring(p3 + 1).toFloat();

        // Store the slot data
        slotData[slotID] = "slot:" + String(slotID) +
                           ", stat:" + String(status) +
                           ", time:" + String(occupiedTime) +
                           ", bill:" + String(bill);
      }
    }

    if (end == -1) break;
    start = end + 1;
  }

  // Rebuild the output message for the UI
  currentMessage = "";
  for (int i = 1; i <= 4; i++) {
    if (slotData[i].length() > 0) {
      currentMessage += slotData[i] + "\n";
    } else {
      currentMessage += "slot:" + String(i) + ", stat:0, time:0, bill:0\n";
    }
  }

  Serial.println("FINAL OUTPUT SENT TO UI:");
  Serial.println(currentMessage);
  server.send(200, "text/plain", "OK");  // Respond to the client with "OK"
}