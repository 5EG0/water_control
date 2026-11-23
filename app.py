"""Web frontend (Flask) untuk simulasi water tank.

Seluruh logika simulasi ada di program C: engine/water_tank.c.
Flask hanya menerima request dari browser, memanggil program C dalam mode
`--api`, lalu meneruskan hasil JSON-nya ke browser.

Vercel bersifat serverless & stateless, jadi state tangki disimpan di browser
dan dikirim ulang pada setiap aksi.
"""
import json
import os
import shutil
import subprocess
import tempfile

from flask import Flask, jsonify, render_template, request

app = Flask(__name__)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
C_SOURCE = os.path.join(BASE_DIR, "engine", "water_tank.c")
IS_WINDOWS = os.name == "nt"
if IS_WINDOWS:
    # Jalan lokal di Windows: pakai water_tank.exe langsung
    C_BINARY = os.path.join(BASE_DIR, "engine", "water_tank.exe")
    RUN_PATH = C_BINARY
else:
    # Linux/Vercel: binary disalin ke /tmp agar bisa diberi izin eksekusi
    C_BINARY = os.path.join(BASE_DIR, "engine", "water_tank")
    RUN_PATH = os.path.join(tempfile.gettempdir(), "water_tank_engine")

ACTIONS = {"start", "stop", "open", "close", "reset", "tick"}
STATE_CODES = {"IDLE": 0, "FILLING": 1, "DRAINING": 2, "FULL": 3, "EMPTY": 4}

DEFAULT_STATE = {
    "waterLevel": 45.0, "inflowRate": 5.0, "outflowRate": 4.0,
    "pumpStatus": 0, "valveStatus": 0, "simulationStatus": 0, "state": "IDLE",
}


def prepare_engine():
    """Salin binary ke folder tulis (/tmp) dan beri izin eksekusi.
    Jika binary tidak ada, coba kompilasi dari sumber C (butuh gcc)."""
    if os.path.exists(RUN_PATH):
        return RUN_PATH
    if os.path.exists(C_BINARY) and not IS_WINDOWS:
        shutil.copyfile(C_BINARY, RUN_PATH)
    elif shutil.which("gcc"):
        subprocess.run(["gcc", "-O2", "-o", RUN_PATH, C_SOURCE], check=True)
    else:
        raise RuntimeError(f"Program C belum dikompilasi ({os.path.basename(C_BINARY)} tidak ada di folder engine).")
    os.chmod(RUN_PATH, 0o755)
    return RUN_PATH


def run_c(action, state):
    """Jalankan program C dan kembalikan hasilnya sebagai dict."""
    args = [
        prepare_engine(), "--api", action,
        repr(float(state["waterLevel"])),
        str(int(state["pumpStatus"])),
        str(int(state["valveStatus"])),
        str(int(state["simulationStatus"])),
        str(STATE_CODES.get(state["state"], 0)),
    ]
    out = subprocess.run(args, capture_output=True, text=True, timeout=5)
    return json.loads(out.stdout)


@app.route("/")
def index():
    return render_template("index.html", state=DEFAULT_STATE)


@app.post("/api/action")
def action():
    data = request.get_json(silent=True) or {}
    act = data.get("action")
    if act not in ACTIONS:
        return jsonify(error="Aksi tidak dikenal"), 400

    state = {**DEFAULT_STATE, **(data.get("state") or {})}
    try:
        result = run_c(act, state)
    except (RuntimeError, ValueError, KeyError, TypeError,
            subprocess.SubprocessError, OSError) as e:
        return jsonify(error=f"Program C gagal dijalankan: {e}"), 500

    message = result.pop("message", "")
    result.pop("stateCode", None)
    return jsonify(state=result, message=message)
