from flask import Flask, request, jsonify
from flask_cors import CORS
import requests
from datetime import datetime

app = Flask(__name__)
CORS(app)

# ----------------- Configuration -----------------
OPENWEATHER_API_KEY = "eca483009d0e5e53599351b8f8f33a30"
LAT = 6.8177
LON = 79.8749
OPENWEATHER_URL = f"https://api.openweathermap.org/data/2.5/weather?lat={LAT}&lon={LON}&appid={OPENWEATHER_API_KEY}"

# ----------------- Current Data -----------------
current_data = {
    "indoor": {"temperature": 0, "humidity": 0},
    "outdoor": {"temperature": 0, "humidity": 0},
    "lightLevel": 0,
    "motion": False,
    "wifi": True,
    "lastUpdate": "",
    "devices": {
        "fan": {"status": False, "manual": False},
        "ac": {"status": False, "manual": False},
        "light": {"status": False, "manual": False}
    }
}

# ----------------- ESP Updates -----------------
@app.route("/api/update", methods=["POST"])
def update_data():
    global current_data
    try:
        esp_data = request.json
        # Update only indoor, light, motion, wifi, lastUpdate, devices
        current_data["indoor"] = esp_data.get("indoor", current_data["indoor"])
        current_data["lightLevel"] = esp_data.get("lightLevel", current_data["lightLevel"])
        current_data["motion"] = esp_data.get("motion", current_data["motion"])
        current_data["wifi"] = esp_data.get("wifi", current_data["wifi"])
        current_data["lastUpdate"] = datetime.now().isoformat()
        current_data["devices"] = esp_data.get("devices", current_data["devices"])
        return jsonify({"status": "ok"}), 200
    except Exception as e:
        return jsonify({"status": "error", "error": str(e)}), 400

# ----------------- React Fetch -----------------
@app.route("/api/status", methods=["GET"])
def get_status():
    # Fetch real-time outdoor weather
    try:
        r = requests.get(OPENWEATHER_URL, timeout=5)
        if r.status_code == 200:
            data = r.json()
            current_data["outdoor"]["temperature"] = data["main"]["temp"] - 273.15
            current_data["outdoor"]["humidity"] = data["main"]["humidity"]
    except:
        pass  # Keep old values if API fails
    return jsonify(current_data), 200

# ----------------- Device Control -----------------
@app.route("/api/device/<device>/<action>", methods=["POST"])
def control_device(device, action):
    if device not in current_data["devices"]:
        return jsonify({"error": "Unknown device"}), 400
    if action not in ["on", "off", "auto"]:
        return jsonify({"error": "Invalid action"}), 400

    if action == "on":
        current_data["devices"][device]["status"] = True
        current_data["devices"][device]["manual"] = True
    elif action == "off":
        current_data["devices"][device]["status"] = False
        current_data["devices"][device]["manual"] = True
    else:  # auto
        current_data["devices"][device]["manual"] = False

    return jsonify(current_data), 200

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
