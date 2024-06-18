import http.server
import socketserver
import socket
import json
import requests

# Global dictionary to store routines
ROUTINES = {
    "1327": ["L_1_ON", "F_1_ON_50"],
    "0830": ["F_1_OFF"]
}
checkbox_states = {
    'light': True,
    'fan': False
}
fan_speed = 3

ESP_ENDPOINT = "http://192.168.225.133/update" 

def send_to_esp(data):
    headers = {'Content-Type': 'application/json'}
    print(data)
    try:
        response = requests.post(ESP_ENDPOINT, data=json.dumps(data), headers=headers)
        print("Sent data to ESP:", data)
        print("Response:", response.text)
    except requests.exceptions.RequestException as e:
        print(f"Error sending data to ESP: {e}")

def update_esp_with_routines():
    global ROUTINES
    send_to_esp({"routines": ROUTINES})

def update_esp_with_checkbox_states():
    global checkbox_states
    send_to_esp({"checkbox_states": checkbox_states})

def update_esp_with_fan_speed():
    global fan_speed
    send_to_esp({"fan_speed": fan_speed})

class RequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()

        with open('D:\\College\\Sem -6\\Code\\index.html', 'rb') as f:
            content = f.read().decode('utf-8')
            content_with_routines = content.replace('{{ ROUTINES }}', json.dumps(ROUTINES))
            self.wfile.write(content_with_routines.encode('utf-8'))

    def do_POST(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/plain')
        self.end_headers()

        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length).decode('utf-8')

        if self.path == '/':
            toggle_checkbox(post_data)
        elif self.path == '/submitroutines':
            submit_routines(post_data)
            update_esp_with_routines()
        elif self.path == '/deleteroutine':
            delete_routine(post_data)

        self.wfile.write(b'Success')

def toggle_checkbox(data):
    global checkbox_states
    try:
        data = json.loads(data)
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON data: {e}")
        return
    if "speed" in data :
        print(data)
    device = data.get("device")
    action = data.get("action")

    if device == 'light' or device == 'fan':
        checkbox_states[device] = (action == 'On')
        print(f"Checkbox '{device}' toggled {'On' if checkbox_states[device] else 'Off'}")
        if "speed" in data:
            send_to_esp({"device":device,"action": "true" if action == "On" else "false","speed":data["speed"]})
        else:
            send_to_esp({"device":device,"action": "true" if action == "On" else "false"})
    else:
        print(f"Invalid device '{device}' received in toggle data")
def submit_routines(data):
    global ROUTINES
    routine_data = json.loads(data)
    time = routine_data.get('time')
    room = routine_data.get('room')
    device = routine_data.get('device')
    action = routine_data.get('action').upper()
    speed = routine_data.get('speed')

    if time and device and action:
        if device == 'Light':
            routine = f"L_{room}_{action}"
        elif device == 'Fan':
            if action == 'ON' and speed:
                routine = f"F_{room}_{action}_{speed}"
            else:
                routine = f"F_{room}_{action}"
        else:
            return

        if time in ROUTINES:
            ROUTINES[time].append(routine)
        else:
            ROUTINES[time] = [routine]

        print(f"Routine added successfully: {routine}")
        update_esp_with_routines()
    else:
        print("Invalid routine data provided")

def delete_routine(data):
    routine_data = json.loads(data)
    time = routine_data.get('time')

    if time in ROUTINES:
        del ROUTINES[time]
        print(f"Routine deleted successfully: {time}")
        update_esp_with_routines()
    else:
        print(f"Routine not found: {time}")

def start_server():
    address = ('', 8001)
    httpd = http.server.HTTPServer(address, RequestHandler) 

    server_ip = socket.gethostbyname(socket.gethostname())
    print(f"Server running on http://{server_ip}:8001/")

    httpd.serve_forever()

if __name__ == "__main__":
    start_server()
