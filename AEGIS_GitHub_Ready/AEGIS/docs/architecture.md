# AEGIS Architecture

## Current prototype

```text
[ESP32 + MPU6500]
        |
        | Wi-Fi
        v
[Node.js / Express Backend]
        |
        +----> Email notification
        |
        v
[Web Dashboard]
```

The current prototype focuses on emergency-event detection, wireless transmission,
backend alert handling, dashboard monitoring, and notification.

## Planned evolution

```text
[AEGIS Wearable]
      |
      | LoRa / multi-hop
      v
[Distributed Mother Systems]
      |
      v
[Fog / Edge Processing]
      |
      v
[Cloud Storage + Analytics]
      |
      v
[Monitoring Dashboard]
      |
      v
[AI-assisted emergency classification]
```

The planned architecture is a future scope and should not be represented as already
implemented in the current prototype.
