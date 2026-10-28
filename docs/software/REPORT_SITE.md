# Report Site (QR code)
 
Publishes a class's attendance report (name, matricula, absences) to a small web
page that the professor opens on their phone by scanning a QR code shown on the
device. The device uploads the data to a cloud database over an external WiFi
network; the page itself is a static HTML file that reads that database.
 
The feature is reachable from the **Export** screen (bottom navigation bar): each
class card has a **REPORT QR CODE** button.
 
## What it produces
 
Two things, per class:
 
1. A record in the cloud database at `/reports/<class code>`.
2. A QR code on the device screen that encodes:
```
https://<PROJECT_ID>.web.app/view.html?code=<class code>
```
 
### Record format
 
```json
{
  "class_name": "ED",
  "limit": 8,
  "expires_at": 1792000000000,
  "students": [
    { "name": "Ana Souza", "id": "202500001", "freq": 2 },
    { "name": "Bruno Lima", "id": "202500002", "freq": 1 }
  ]
}
```

- **`name`**, **`id`** — taken verbatim from the registry
  (`/students/students.json`).
- **`expires_at`** — expiration time as Unix time in milliseconds. The firmware
  sets it to 15 minutes after the device's RTC timestamp.
- **`freq`** — the number of **session days the student was absent**, the same
  value the CSV export writes (see `EXPORT.md`).
- **`limit`** — the absence limit for the class (see below). `0` means no limit
  is configured.
There is exactly one entry per student enrolled in the class, in roster order.
 
### How FREQ and the limit are computed
 
`freq` comes from `export_compute_freq()`, the same function the CSV export uses,
so the number on the page can never drift from the exported file:
 
```
FREQ(student) = (number of session dates) - (session dates the student was present)
```
 
`limit` is 25% of the class's planned sessions, truncated:
 
```
limit = (total_classes * 25) / 100      // integer division
```
 
`total_classes` is calculated at load time from the semester start/end dates
and the class's weekly session windows in `class.json`. A class without them has
`total_classes = 0`, so `limit = 0` and the page shows the bare absence count.
 
## The page
 
`view.html` reads `?code=` from the URL, fetches `/reports/<code>.json` and draws:
 
- A title: `<class name> - <class code>`.
- A table with **Nome**, **Matricula** and **Faltas / Limite**. Faltas / Limite reads `freq/limit` when
  a limit exists, otherwise just `freq`.
- A **Baixar CSV** button with each student's matricula and absence count.
## Using it
 
### Firebase setup

The firmware is preconfigured to upload reports to the project's existing
Realtime Database and to show a QR code pointing to the project's existing
Firebase Hosting site. No Firebase setup is required for the default deployment.

If you want to host the report page under your own Google account, create a
Firebase project, enable Hosting, and deploy the repository's `public/` folder
with `firebase deploy --only hosting`. The Hosting URL is separate from the
Realtime Database URL.

After deploying, update the Hosting URL in `src/ui/screens/scr_export.cpp`
(`qr_button_cb`). The page will still read from the existing Realtime Database
unless you also configure your own database.

To use your own Realtime Database as well (in addition to your own Hosting):

1. Enable Realtime Database in your Firebase project.
2. In `public/view.html`, replace the Realtime Database URL in the `fetch()`
   call with your database's URL. This controls where the page reads reports.
3. In `src/services/report_service.cpp`, set `HOST` to your database host and
   `SECRET` to the credential the device uses to upload reports. This controls
   where the firmware writes reports.
4. Configure the database rules to: 
```json
{
  "rules": {
    "reports": {
      "$reportId": {
        ".read": "data.child('expires_at').isNumber() && data.child('expires_at').val() > now"
      }
    },
    ".write": false
  }
}
```

The device uploads each report with Firebase authentication. Keep the database
credential private and do not commit your own credential to a public repository.
 
### Flow on the device
 
1. **WiFi screen -> Connect to an external network.** Enter the SSID and password
   (the eye button reveals what was typed) and tap **Connect**. The status line
   shows the device IP once connected.
2. **Export screen.** Tap **REPORT QR CODE** on the class card.
3. If the device is not connected to a network, a toast asks for that first.
   Otherwise the report is uploaded (overwriting `/reports/<code>`) and a modal
   titled with the class name shows the QR code.
4. The professor scans the QR code. The page opens in the phone's browser, which
   needs its own internet access - it does **not** need to be on the device's
   WiFi.
## Behavior notes
 
- **Snapshot, not live.** The page shows the data as of the last time the QR
  button was tapped. Tap it again to refresh; each upload fully replaces the
  previous record for that class.
- **Open roll call is preserved.** Computing FREQ goes through
  `export_compute_freq()`, which snapshots and restores any attendance session in
  progress (same guarantee as the CSV export).
- **The UI blocks while uploading.** The HTTPS request runs on the UI thread, and
  FREQ re-reads every session file of the class first, so expect a short freeze
  that grows with the number of recorded session days.
- **Connecting to an external network turns the device's own access point off**
  (and the file server with it). The two are not used at the same time; start the
  access point again from the WiFi screen when needed.
- **The database secret lives in the firmware.** Do not commit it to a public
  repository.
- **Reports expire after 15 minutes.** Each successful upload sets `expires_at`
  to 15 minutes after the device's RTC timestamp. The page checks this value
  against the phone's current time and shows `Relatorio Expirado` when the
  report has expired. If no report exists for the class code (or it has no
  `expires_at` value), the page shows the same message. Uploading the report
  again refreshes its expiration time. The upload fails if the device's RTC
  timestamp is unavailable.
  
## Troubleshooting
 
| Symptom | Likely cause |
| --- | --- |
| Log shows `Reason: 201 - NO_AP_FOUND` | The network was not seen in the scan. Check the SSID spelling (case and spaces). The WiFi companion chip is 2.4 GHz only. For an iPhone hotspot, keep the Personal Hotspot screen open and enable **Maximize Compatibility**. |
| Page shows `Relatorio Expirado` immediately after upload | The report may not exist, may not include a valid `expires_at`, or may be unreadable under the database rules. Check the database URL and read rules. Also check that the device and phone clocks are correct. |
| "Problem sending data" toast | The upload failed. Check the WiFi connection, `HOST` and `SECRET`, database write authorization/rules, and whether the device has a valid RTC timestamp. A normal auth token cannot write when `.write` is `false`; only a credential with sufficient privileges can authorize the PUT. |
| QR opens an expired report | Reports expire 15 minutes after upload. Tap **REPORT QR CODE** again to upload a fresh report, then scan the new QR code. |
 
## Where it lives (for developers)
 
| Piece | File |
| --- | --- |
| Report upload (Firebase `PUT`) | `src/services/report_service.cpp`, `include/services/report_service.h` |
| FREQ computation | `export_compute_freq()` in `src/services/export_service.cpp` |
| External WiFi (STA) | `src/services/wifi_extern.cpp`, `include/services/wifi_extern.h` |
| WiFi screen (SSID/password, Connect) | `src/ui/screens/scr_wifi_editor.cpp` |
| QR button + modal | `src/ui/screens/scr_export.cpp` (`qr_button_cb`) |
| Public page | `view.html` in the Firebase Hosting `public/` folder |
 
### API
 
```c
// report_service.h
// Uploads cls's report (name, matricula, freq per student, and the absence
// limit) to /reports/<code>.json, requires wifi connected
bool report_upload(const class_rec_t* cls);
 
// wifi_extern.h
// Connects to an external network, stopping the device's own access point
// first. Non-blocking: poll wifi_sta_is_connected() for the result.
void wifi_connect(const char* name, const char* password);
void wifi_disconnect(void);
bool wifi_is_connected(void);

// "" when not connected.
void wifi_ip(char* out, size_t cap);

// saves wifi info for next connection
void wifi_restore(void);
bool wifi_get_saved_credentials(char* name, size_t name_cap, char* password, size_t password_cap);
```
 