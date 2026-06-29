"""
Minimal HTTP server that exposes CPU temperature from Libre Hardware Monitor WMI.
Runs alongside Glances on port 61209.
ESP32 queries: http://192.168.99.20:61209/api/temp
"""
import json
import http.server
import asyncio
import wmi
import threading

HOST = "0.0.0.0"
PORT = 61209

def get_cpu_temp():
    """Read CPU Package temperature from Libre Hardware Monitor WMI."""
    try:
        c = wmi.WMI(namespace="root/LibreHardwareMonitor")
        sensors = c.Sensor()
        for s in sensors:
            if s.SensorType == "Temperature" and s.Name == "CPU Package":
                if s.Value is not None and s.Value > 0:
                    return round(float(s.Value), 1)
            # Fallback: try "Core Max" or "CPU Core #1"
            if s.SensorType == "Temperature" and s.Name == "Core Max":
                if s.Value is not None and s.Value > 0:
                    return round(float(s.Value), 1)
    except Exception as e:
        return None
    return None

class TempHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/api/temp":
            temp = get_cpu_temp()
            if temp is not None:
                data = json.dumps([{"label": "CPU Package", "value": temp, "unit": "C"}])
            else:
                data = json.dumps([])
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(data.encode())
        elif self.path == "/health":
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"OK")
        else:
            self.send_response(404)
            self.end_headers()
    def log_message(self, format, *args):
        pass  # Suppress logs

if __name__ == "__main__":
    server = http.server.HTTPServer((HOST, PORT), TempHandler)
    print(f"WMI Temperature server running on http://{HOST}:{PORT}/api/temp")
    server.serve_forever()