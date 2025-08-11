from flask import Flask, render_template
import requests

app = Flask(__name__)

BACKEND_URL = 'http://localhost:3000' #node.js backend url

@app.route('/')
def index():
    try:
        response = requests.get(f'{BACKEND_URL}/device')
        devices = response.json()
    except Exception as e:
        print("Error fetching devices:", e)
        devices = []

    return render_template('index.html', devices=devices)

@app.route("/device/<int:device_id>/images")
def view_device_images(device_id):
    try:
        response = requests.get(f'{BACKEND_URL}/device/{device_id}/images')
        response.raise_for_status()  
        images = response.json()
    except requests.RequestException as e:
        print(f"Error fetching images: {e}")
        images = []  
    return render_template("images.html", images=images, device_id=device_id, BACKEND_URL=BACKEND_URL)


@app.template_filter('toronto_time')
def toronto_time(value, fmt='%Y-%m-%d %H:%M:%S'):
    from datetime import datetime, timezone
    from zoneinfo import ZoneInfo
    if isinstance(value, str):
        dt = datetime.fromisoformat(value.replace('Z', '+00:00'))
    elif isinstance(value, (int, float)):
        if value > 1e12: value /= 1000.0  # ms → s
        dt = datetime.fromtimestamp(value, tz=timezone.utc)
    else:
        dt = value if value.tzinfo else value.replace(tzinfo=timezone.utc)
    return dt.astimezone(ZoneInfo('America/Toronto')).strftime(fmt)

if __name__ == '__main__':
    app.run(debug=True, port=4000)
