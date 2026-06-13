from fastapi import FastAPI, WebSocket, Request, WebSocketDisconnect
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse
import uvicorn
import json
import logging
from typing import List

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("cryo_sentinel")

app = FastAPI(title="Cryo Sentinel Enterprise Backend")

class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info(f"Client connected. Total clients: {len(self.active_connections)}")

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)
        logger.info("Client disconnected.")

    async def broadcast(self, message: dict):
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except Exception as e:
                logger.error(f"Error sending to client: {e}")

manager = ConnectionManager()

# Mock initial state to send to clients if needed, otherwise dashboard handles it
current_state = {}

@app.post("/api/webhook")
async def ttn_webhook(request: Request):
    """
    Receives LoRaWAN Webhooks from The Things Network (Cayenne LPP decoded).
    """
    try:
        data = await request.json()
        logger.info(f"Received TTN Webhook: {json.dumps(data)}")
        
        # Extract Cayenne LPP payload from TTN format
        # TTN typically sends decoded payload in `uplink_message.decoded_payload`
        uplink = data.get("uplink_message", {})
        decoded = uplink.get("decoded_payload", {})
        
        if not decoded:
            return {"status": "ignored", "reason": "no decoded payload"}
            
        # Parse the Cayenne LPP decoded data into our dashboard format
        dashboard_event = {
            "type": "telemetry",
            "device_id": data.get("end_device_ids", {}).get("device_id", "cryosentinel-01"),
            "temp_c": decoded.get("temperature_1", 0.0),
            "lat": decoded.get("gps_2", {}).get("latitude", 0.0),
            "lon": decoded.get("gps_2", {}).get("longitude", 0.0),
            "breach_type": decoded.get("digital_in_3", 0),
            "timestamp": data.get("received_at", "")
        }
        
        # Broadcast the event to all connected dashboard clients
        await manager.broadcast(dashboard_event)
        
        return {"status": "success"}
    except Exception as e:
        logger.error(f"Error processing webhook: {e}")
        return {"status": "error"}

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            # Keep connection alive
            data = await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)

# Mount the static docs folder so we can serve demo_dashboard.html
import os
docs_path = os.path.join(os.path.dirname(__file__), "..", "docs")
app.mount("/", StaticFiles(directory=docs_path, html=True), name="static")

if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)
