# AEGIS — Intelligent Emergency & Safety System

> **AEGIS** is a compact safety-system prototype designed to detect emergency events,
> transmit alerts, and provide centralized monitoring through a web dashboard.

## Overview

AEGIS combines embedded sensing, wireless communication, backend alert processing,
notifications, and live monitoring into one safety-oriented system.

### Current prototype

```text
Wearable / ESP32
      ↓
MPU6500-based motion detection
      ↓
Wi-Fi
      ↓
Node.js + Express backend
      ↓
Web dashboard + email notification
```

The repository contains the working prototype software and documentation. The broader
LoRa, distributed Mother System, fog-computing, cloud, and AI architecture is documented
as **future scope**, not as a claim about the current implementation.

## Features

- ESP32-based embedded prototype
- MPU6500 motion sensing
- Emergency/shake-event detection
- Wireless alert transmission over Wi-Fi
- Node.js / Express backend
- Alert endpoint and alert history
- Alert resolution workflow
- Web-based monitoring dashboard
- Email notification support
- Clear separation between current implementation and future architecture

## Repository Structure

```text
AEGIS/
├── README.md
├── LICENSE
├── .gitignore
├── firmware/
│   └── esp32/
├── backend/
│   ├── server.js
│   ├── package.json
│   ├── package-lock.json
│   └── .env.example
├── dashboard/
│   └── index.html
├── assets/
└── docs/
    └── architecture.md
```

## Hardware

The prototype uses:

- ESP32 development board
- MPU6500 IMU
- Vibration motor
- IRLZ44N MOSFET for motor switching
- Supporting resistors, wiring, and power components

> Hardware configuration may change as AEGIS evolves toward a smaller wearable form factor.

## Software Stack

**Firmware**
- Arduino / ESP32
- C/C++

**Backend**
- Node.js
- Express
- Email notification service

**Frontend**
- HTML
- CSS
- JavaScript

## Getting Started

### 1. Firmware

Open the ESP32 firmware under:

```text
firmware/esp32/
```

Before flashing, replace the placeholder Wi-Fi values with your own local credentials.

Do **not** commit real passwords or API credentials.

### 2. Backend

From the backend directory:

```bash
cd backend
npm install
```

Create your local environment file:

```bash
copy .env.example .env
```

Then fill in the required values in `.env`.

Start the server using the command defined in `package.json`, for example:

```bash
npm start
```

### 3. Dashboard

Open:

```text
dashboard/index.html
```

If the dashboard expects a specific backend address, configure it locally rather than
committing a private LAN address.

## Security

This repository intentionally contains **no real Wi-Fi passwords, email passwords,
API keys, or other private credentials**.

If a credential has previously been committed to any public repository, revoke/rotate it
before publishing the cleaned repository.

## Current vs Future Scope

### Implemented prototype

- ESP32 sensing
- MPU6500-based motion/event detection
- Wi-Fi communication
- Backend alert processing
- Dashboard monitoring
- Email notification

### Planned

- LoRa-based communication
- Multi-hop / store-and-forward communication
- Distributed Mother Systems
- Fog/edge processing
- Cloud-scale data storage
- AI-assisted emergency classification
- More advanced fall/emergency detection
- Further miniaturization for wearable deployment

## Recognition

AEGIS received **1st Prize** at an IETE Eastern Zonal competition focused on
**AI for Sustainable Society**.

## Disclaimer

AEGIS is a prototype/research project. It should not be treated as a certified life-safety
device without appropriate validation, testing, redundancy, regulatory review, and
certification.

## License

This project is released under the MIT License. See `LICENSE`.
