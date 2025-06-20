# API

A lightweight HTTP server written in Go to ingest JSON sensor data from remote ESP32 + SIM7600 nodes and store it in PostgreSQL.

## 📦 Features

- Accepts JSON payloads via HTTP `POST /ingest`
- Stores data in PostgreSQL (`node_id`, `value`, `type`, `timestamp`)
- Easy to deploy as a binary or container

## 📐 JSON Payload Format

```json
{
  "node_id": "sensor-001",
  "timestamp": 1718901234,
  "value": 29.4,
  "type": "soil_moisture"
}
