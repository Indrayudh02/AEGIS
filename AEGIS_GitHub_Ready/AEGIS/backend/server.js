// Silent SOS — Backend
// Minimal Express server: receives alerts (from the ESP32 wearable, or the
// dashboard's "Simulate Trigger" button while hardware isn't ready yet),
// stores them in memory, and lets the dashboard poll for current status.

require('dotenv').config();
const express = require('express');
const app = express();
const PORT = process.env.PORT || 3000;

app.use(express.json());

// ---------- Email alert setup (Nodemailer + Gmail) ----------
// Reads credentials from .env (see .env.example). If they're missing,
// email sending is skipped and a warning is logged once at startup —
// the rest of the app (alerts, dashboard, log) still works fine without it.
const nodemailer = require('nodemailer');
const {
  EMAIL_USER,
  EMAIL_APP_PASSWORD,
  EMERGENCY_EMAILS, // comma-separated, e.g. contact1@example.com,contact2@example.com
} = process.env;

let mailTransporter = null;
const emailReady = EMAIL_USER && EMAIL_APP_PASSWORD;

if (emailReady) {
  mailTransporter = nodemailer.createTransport({
    service: 'gmail',
    auth: { user: EMAIL_USER, pass: EMAIL_APP_PASSWORD },
  });
} else {
  console.warn('[Email] EMAIL_USER or EMAIL_APP_PASSWORD missing in .env — email alerts are disabled. Alerts will still be logged and shown on the dashboard.');
}

async function sendEmergencyEmail(alert) {
  if (!emailReady) return;

  const contacts = (EMERGENCY_EMAILS || '').split(',').map(e => e.trim()).filter(Boolean);
  if (contacts.length === 0) {
    console.warn('[Email] No EMERGENCY_EMAILS configured in .env — skipping email.');
    return;
  }

  const locationLabel = alert.location.label || 'an unknown location';
  const timeLabel = new Date(alert.timestamp).toLocaleString();

  try {
    const info = await mailTransporter.sendMail({
      from: `"Silent SOS" <${EMAIL_USER}>`,
      to: contacts.join(','), // one email, multiple recipients
      subject: `🚨 SILENT SOS ALERT — ${alert.deviceId}`,
      text: `SILENT SOS ALERT\n\nDevice: ${alert.deviceId}\nLocation: ${locationLabel}\nTime: ${timeLabel}\nTrigger type: ${alert.triggerType}\n\nThis is an automated alert from a Silent SOS wearable device.`,
      html: `
        <div style="font-family: sans-serif; padding: 20px; background: #0A0F1C; color: #EDF1F7;">
          <h2 style="color: #F3374A; margin: 0 0 12px;">🚨 SILENT SOS ALERT</h2>
          <p><strong>Device:</strong> ${alert.deviceId}</p>
          <p><strong>Location:</strong> ${locationLabel}</p>
          <p><strong>Time:</strong> ${timeLabel}</p>
          <p><strong>Trigger type:</strong> ${alert.triggerType}</p>
          <p style="color: #7C8AA5; font-size: 12px; margin-top: 20px;">Automated alert from a Silent SOS wearable device.</p>
        </div>
      `,
    });
    console.log(`[Email] Alert sent to ${contacts.join(', ')} (id: ${info.messageId})`);
  } catch (err) {
    console.error('[Email] Failed to send:', err.message);
  }
}

// Allow the dashboard (opened as a local file, or served from anywhere)
// to call this API. Keeping this manual instead of adding the `cors`
// package as a dependency — one less thing to install for a hackathon.
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Methods', 'GET,POST,OPTIONS');
  res.header('Access-Control-Allow-Headers', 'Content-Type');
  if (req.method === 'OPTIONS') return res.sendStatus(200);
  next();
});

// ---------- In-memory state ----------
// Swap this for a real database later if you need alerts to survive a
// server restart — not necessary for a hackathon demo.
let alerts = [];
let activeAlert = false;
let nextId = 1;

// ---------- Routes ----------

// Health check — useful to confirm the server is up before wiring hardware
app.get('/', (req, res) => {
  res.json({ status: 'ok', message: 'Silent SOS backend running' });
});

// ESP32 (or the dashboard's simulate button) calls this to raise an alert
app.post('/alert', (req, res) => {
  const { deviceId, location, triggerType } = req.body;

  const alert = {
    id: nextId++,
    deviceId: deviceId || 'SOS-ESP32-A1',
    location: location || { lat: null, lng: null, label: 'Unknown location' },
    triggerType: triggerType || 'manual',
    timestamp: new Date().toISOString(),
  };

  alerts.unshift(alert); // newest first
  activeAlert = true;

  console.log(`[ALERT] ${alert.deviceId} — ${alert.timestamp}`);

  // Fire the email but don't make the API response wait on it — the
  // dashboard should update instantly even if sending is slow or fails
  sendEmergencyEmail(alert).catch(err => console.error('[Email] Unexpected error:', err));

  res.status(201).json(alert);
});

// Dashboard calls this when the alert is manually resolved (e.g. after
// confirming the wearer is safe)
app.post('/resolve', (req, res) => {
  activeAlert = false;
  res.json({ activeAlert });
});

// Dashboard polls this to know current state + alert history
app.get('/status', (req, res) => {
  res.json({ activeAlert, alerts });
});

// Full alert history on its own, if needed separately
app.get('/alerts', (req, res) => {
  res.json(alerts);
});

app.listen(PORT, () => {
  console.log(`Silent SOS backend listening on http://localhost:${PORT}`);
});
