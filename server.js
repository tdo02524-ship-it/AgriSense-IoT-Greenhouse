const express = require("express");// Express.js xây dựng web phổ biến của node.js, xây dựng API và định tuyến Routes
/*bạn mới có thể tạo ra các đường dẫn như /login (để đăng nhập), /api/settings (cài đặt nhiệt độ), /api/sensor/all (lấy lịch sử dữ liệu)., giúp máy tính hiểu và phục vụ fiel giao diện */
const http = require("http");// tạo một server mạng tiêu chuẩn, dùng chạy thời gian thực 
const { Server } = require("socket.io"); // Socket.io (Giao thức truyền thông thời gian thực bằng WebSockets)., nhờ thư viện này esp32 gửi lệnh server, server lập tức dung io.emit() gưi rleen web
const mqtt = require("mqtt"); // biến server thành 1 mqtt client để kêt nối broker, lắng nghe topic, đồng thời để gửi gói data xuông vdk
const path = require("path");
const sqlite3 = require("sqlite3").verbose(); // hệ quản tri csdl

const app = express(); // 1 bộ định tuyến router , định nghĩa các đường dẫn API, xử lí yêu cầu HTTP và quản lý các thư mục chứa giao diện Web tĩnh (public).
const server = http.createServer(app); // tạo ra một web server vật lí , truyền biên sapp
/*Việc bọc app vào trong http.createServer giúp tạo ra một bề mặt Server vững chắc, vừa hiểu được các lệnh Web của Express, */
const io = new Server(server);// Nó cho phép trình duyệt Web và Server giữ một kết nối mở liên tục
const PORT = 3000;

// === 1. KHỞI TẠO DATABASE SQLITE ===
const db = new sqlite3.Database('farm_data_old.db'); // kiểm tra xem tệp tồn tạo chưa
db.serialize(() => { // ép sql chạy theo thứ tự 
    // Nếu bảng này chưa có thì hãy tạo mới, còn nếu hệ thống đã chạy từ trước và có bảng này rồi thì giữ nguyên dữ liệu cũ, cấm được xóa"
    db.run(`CREATE TABLE IF NOT EXISTS sensor_data ( 
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        temperature REAL,
        humidity REAL,
        created_at DATETIME DEFAULT (datetime('now','localtime')) 
    )`);
    // DETATIME tựu động lấy tie máy tính
    
    db.run(`CREATE TABLE IF NOT EXISTS alert_history (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        zone TEXT, 
        reason TEXT, 
        value REAL, 
        level TEXT,
        created_at DATETIME DEFAULT (datetime('now','localtime'))
    )`);
    console.log("👉 SQLite Database & Tables đã sẵn sàng");
});
// khơi tọa biến trnagjt hái toan cuc
let devices = {}; // quan li danh sach thiet bi, giúp quản lí được nhiều trạm, không riêng lẻ
let alertState = { isHighTemp: false, isLowTemp: false }; // khóa chống trùng, lỗi này ghi nhận không ghi nữa
const TIMEOUT = 5000; 

let lastDesiredStates = { bom1: 'OFF', quat1: 'OFF', den1: 'OFF', khac: 'OFF' };// biến nhớ trạng thái công tắc, lưu trạng thái cuối cùng
let lastGreenhouseMode = 'AUTO'; // ghi nhớ chế dộ hiện tại

// Cấu hình thư mục tĩnh public và Route trả về file index.html đúng yêu cầu
app.use(express.static(path.join(__dirname, "public"))); // __dirmane biến hệ thống chứa đường dẫn thư mục hien tai , biến thư mục public thành nới chứa file
/*Nó chỉ ra rằng toàn bộ "vỏ" của giao diện (gồm giao diện đồ thị, các nút bấm bật/tắt bơm, đèn) đang nằm trong một thư mục tên là public */
app.use(express.json()); // thực hiện ccas thao tác tren web đóng gió json bắn lên server
// khong có gì gọi rep.body.email pử api login báo lỗi 
app.get("/", (req, res) => {
    // lắng nghe yêu cầu truy cập băng get
    res.sendFile(path.join(__dirname, "public", "index.html")); // đọc file , bắn nội dung lên web
});

// === 2. API ĐĂNG NHẬP ===
app.post("/login", (req, res) => {
    // data giấu tron boby của goi tin mạng không show trên url 
    // trình duyệt sẽ bắn một gói tin đến địa chỉ: http://27.71.25.224:3000/login.
    const { email, password } = req.body;
    /*Nhờ lệnh app.use(express.json()) ở phía trên, chuỗi văn bản JSON từ Web gửi lên đã được dịch thành đối tượng req.body. Dòng lệnh này sẽ tự động tìm bên trong req.body */
    if ((email === "dongocbaotam@gmail.com" && password === "baotam.1303.") || (email === "123" && password === "123")) {
        res.json({ success: true, user: "Admin" });
    } else { res.json({ success: false }); }
});

app.post("/api/settings", (req, res) => {
    const { tempMin, tempMax, humMin, humMax } = req.body;
    // req chauw toàn bộ thông tin , chứa các hàm và phương thức để Server trả lời hoặc bắn dữ liệu ngược lại cho Trình duyệt sau khi đã xử lý xong yêu cầu
    const configPayload = {
        // từ khóa định dạng gói tin 
        type: "CONFIG",
        tMin: parseFloat(tempMin),
        tMax: parseFloat(tempMax),
        hMin: parseFloat(humMin),
        hMax: parseFloat(humMax)
    };

    client.publish("farm/control", JSON.stringify(configPayload));
    // Nó biến toàn bộ đối tượng JavaScript thành một chuỗi văn bản thuần túy (String) có dạng
    console.log("Đã bắn thông số cài đặt xuống ESP32:", configPayload);
    res.json({ success: true });
});

// === 3. CÁC API TRUY VẤN LỊCH SỬ ===
app.get("/api/sensor/all", (req, res) => {
    db.all("SELECT * FROM sensor_data ORDER BY created_at DESC LIMIT 100", [], (err, rows) => {
        if (err) return res.status(500).json({ error: err.message }); res.json(rows);
    });/*Hàm Callback (err, rows) => { ... }: Chờ database lục tìm xong thì sẽ trả về kết quả vào hàm này. Nếu lỗi thì dính vào err, nếu thành công thì dữ liệu nằm trong rows. */
});//ORDER BY created_at DESC: Sắp xếp dữ liệu theo thứ tự Giảm dần , mới thì lên trên 
app.get("/api/sensor/today", (req, res) => {
    db.all("SELECT * FROM sensor_data WHERE date(created_at) = date('now', 'localtime') ORDER BY created_at DESC", [], (err, rows) => {
        if (err) return res.status(500).json({ error: err.message }); res.json(rows);
    });
});
app.get("/api/sensor/3days", (req, res) => {
    db.all("SELECT * FROM sensor_data WHERE created_at >= datetime('now', '-3 days', 'localtime') ORDER BY created_at DESC", [], (err, rows) => {
        if (err) return res.status(500).json({ error: err.message }); res.json(rows);
    });
});
app.get("/api/sensor/week", (req, res) => {
    const sql = `SELECT * FROM sensor_data WHERE strftime('%W', created_at) = strftime('%W', 'now', 'localtime') AND strftime('%Y', created_at) = strftime('%Y', 'now') ORDER BY created_at DESC`;
    db.all(sql, [], (err, rows) => { if (err) return res.status(500).json({ error: err.message }); res.json(rows); });
});
app.get("/api/sensor/month", (req, res) => {
    const sql = `SELECT * FROM sensor_data WHERE strftime('%m', created_at) = strftime('%m', 'now', 'localtime') AND strftime('%Y', created_at) = strftime('%Y', 'now') ORDER BY created_at DESC`;
    db.all(sql, [], (err, rows) => { if (err) return res.status(500).json({ error: err.message }); res.json(rows); });
});
app.get("/api/sensor/avg-today", (req, res) => {
    const sql = `SELECT AVG(temperature) AS avg_temp, AVG(humidity) AS avg_humidity FROM sensor_data WHERE date(created_at) = date('now', 'localtime')`;
    db.get(sql, [], (err, row) => { if (err) return res.status(500).json({ error: err.message }); res.json(row); });
});
app.get("/api/sensor/min-max", (req, res) => {
    const sql = "SELECT MAX(temperature) AS max_temp, MIN(temperature) AS min_temp FROM sensor_data";
    db.get(sql, [], (err, row) => { if (err) return res.status(500).json({ error: err.message }); res.json(row); });
});
app.get("/api/alerts", (req, res) => {
    db.all("SELECT * FROM alert_history ORDER BY created_at DESC LIMIT 50", [], (err, rows) => {
        if (err) return res.status(500).json({ error: err.message }); res.json(rows);
    });
});

// === 4. MQTT (SSL 8883) ===
const client = mqtt.connect("mqtts://27.71.25.224:8883", { rejectUnauthorized: false });
// sự kiện kết nói thành công mqtt broker server
client.on("connect", () => {
    console.log("🚀 MQTTS Connected");
    client.subscribe("farm/sensor"); // nhan data ic lien tuc, bóc tách lư sql và dùng socket.io đẩy
    client.subscribe("farm/sync"); // don nhan mach esp32 vua khoi dong lai
    client.subscribe("farm/sync_update"); // nhận du lieuj đông bojojo khi nút nhấn vật lí thay đổi
    client.subscribe("farm/wifi_results"); // Đăng ký nhận kết quả quét mạng từ ESP32
});

client.on("error", (err) => {
    console.error("❌ MQTTS Error:", err.message);
});

client.on("message", (topic, message) => {
    try { // mở vùng án nếu lỗi cú pháp thì try gánh không sập web
        // === XỬ LÝ DANH SÁCH QUÉT WIFI TỪ ESP32 ===
        if (topic === "farm/wifi_results") {
            const wifiList = JSON.parse(message.toString());//Chuyển đổi gói tin thô dạng mảng byte thành một chuỗi chữ (String) đọc được.
            console.log("📶 Danh sách mạng quét được từ ESP32:", wifiList);
            // Gửi danh sách mạng này thẳng lên giao diện Web qua Socket.io
            io.emit("wifi_list_update", wifiList); // đưa lên web
            return;
        }

        if (topic === "farm/sync_update") {
            const data = JSON.parse(message.toString());
            console.log("🔄 Đồng bộ phần cứng từ ESP32:", data);
            
            if (data.mode) {
                lastGreenhouseMode = data.mode;
                io.emit("relay_status_update", { mode: data.mode });
            }
            if (data.device) { 
                lastDesiredStates[data.device] = data.state; 
                io.emit("dong_bo_cong_tac", data);
            }
            return;
        }

        if (topic === "farm/sync") {
            console.log("🔄 Mạch trực tuyến trở lại, tiến hành đồng bộ trạng thái...");
            client.publish("farm/control", JSON.stringify({ mode: lastGreenhouseMode }));
            
            let delayMs = 200;
            for (let dev in lastDesiredStates) {
                setTimeout(() => {
                    client.publish("farm/control", JSON.stringify({ device: dev, state: lastDesiredStates[dev] }));
                }, delayMs);
                delayMs += 200;
            }// gửi các lệnh điều khiển 0.2s
            return;
        }

        const json = JSON.parse(message.toString());
        const deviceId = json.id || "DEVICE_1";
        const temp = parseFloat(json.temp);
        const humi = parseFloat(json.hum);
        const lat = json.lat || 10.762622;
        const lon = json.lon || 106.660172;
        const net = json.net || "N/A"; 
        
        devices[deviceId] = { temp: temp, humi: humi, lat: lat, lon: lon, net: net, last_seen: Date.now() };

        let epochTime = parseInt(json.epoch); // ep data json.epoch nhan gói tin mqtt cưa esp, hàm parsetint ép chuối vè nguyên 
        if (epochTime < 1000000000) epochTime += 946684800; // y2k 1000000000 đại diện 9/2001
        // lấy mốc 2000, esp tính toán hụt 30 năm (1970), 
        epochTime -= (7 * 3600); // bù trừ lệnh giwog múi gmt+7

        let millisecondTimestamp = epochTime * 1000;

        const stmt = db.prepare("INSERT INTO sensor_data (temperature, humidity, created_at) VALUES (?, ?, datetime(?, 'unixepoch', 'localtime'))");
        stmt.run(temp, humi, epochTime, function(err) {
            if(err) console.error("❌ Lỗi ghi DB: ", err.message);
        });
        stmt.finalize();

        let currentUnix = Math.floor(Date.now() / 1000);
        let isOfflineData = (currentUnix - epochTime > 5); 

        if (!isOfflineData) {
            io.emit("sensor_data", { temp, humi, lat, lon, net, timestamp: millisecondTimestamp, isOfflineData: false });
            console.log(`[Realtime] Nhiệt độ: ${temp}°C - Độ ẩm: ${humi}% - Mạng: ${net}`);
        } else {
            io.emit("sensor_data", { temp, humi, lat, lon, net, timestamp: millisecondTimestamp, isOfflineData: true });
            console.log(`[Xả kho] Nhiệt độ: ${temp}°C - Độ ẩm: ${humi}% - Mạng: ${net}`);
        }

        if (temp >= 30 && !alertState.isHighTemp) {
            alertState.isHighTemp = true; 
            db.run("INSERT INTO alert_history (zone, reason, value, level, created_at) VALUES ('Khu vuc Nong nghiep', 'Nhiet do qua cao', ?, 'Canh bao', datetime(?, 'unixepoch', 'localtime'))", [temp, epochTime]);
        } else if (temp < 30) { alertState.isHighTemp = false; }

        if (temp <= 15 && !alertState.isLowTemp) {
            alertState.isLowTemp = true;
            db.run("INSERT INTO alert_history (zone, reason, value, level, created_at) VALUES ('Khu vuc Nong nghiep', 'Nhiet do qua thap', ?, 'Nguy hiem', datetime(?, 'unixepoch', 'localtime'))", [temp, epochTime]);
        } else if (temp > 15) { alertState.isLowTemp = false; }

    } catch (e) {
        console.log("⚠️ Dữ liệu MQTTS không hợp lệ:", message.toString());
    }
});

function getStatus(device) {
    return (Date.now() - device.last_seen < TIMEOUT) ? "ONLINE" : "OFFLINE";
}

setInterval(() => {
    let result = {};
    for (let id in devices) { result[id] = { ...devices[id], status: getStatus(devices[id]) }; }
    io.emit("device_status", result);
}, 2000);

// === 5. SOCKET.IO (Giao tiếp Web và Server) ===
io.on("connection", (socket) => {
    console.log("💻 Web client đã kết nối qua Socket.io");
    
    socket.on("control_device", (data) => {
         if (data.device) { lastDesiredStates[data.device] = data.state; }
         if (data.mode) { lastGreenhouseMode = data.mode; } 
         client.publish("farm/control", JSON.stringify(data));
         io.emit("dong_bo_cong_tac", data);
     });

     // === XỬ LÝ YÊU CẦU THAO TÁC WI-FI TỪ WEB ===
     socket.on("wifi_request", (data) => {
         // Nhận lệnh { action: "scan_wifi" } hoặc { action: "connect_wifi", ssid: "...", pass: "..." }
         console.log("📢 Web yêu cầu thao tác mạng:", data);
         // Bắn lệnh dưới dạng JSON string trực tiếp xuống topic điều khiển của ESP32
         client.publish("farm/control", JSON.stringify(data));
     });
});

server.listen(PORT, "0.0.0.0", () => {
    console.log(`🌐 Hệ thống đang chạy tại: http://27.71.25.224:${PORT}`);
});