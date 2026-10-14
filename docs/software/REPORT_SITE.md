# Report Site (QR code)
 
Publishes a class's attendance report (name, matricula, absences) to a small web
page that the professor opens on their phone by scanning a QR code shown on the
device. The device uploads the data to a cloud database over an external WiFi
network; the page itself is a static HTML file that reads that database.
 
The feature is reachable from the **Export** screen (bottom navigation bar): each
class card has a **QR do CSV** button.
 
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
  "students": [
    { "name": "Ana Souza", "id": "202500001", "freq": 2 },
    { "name": "Bruno Lima", "id": "202500002", "freq": 1 }
  ]
}
```
 
- **`name`**, **`id`** — taken verbatim from the registry
  (`/students/students.json`).
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
limit = (planned_sessions * 25) / 100      // integer division
```
 
`planned_sessions` is calculated at load time from the semester start/end dates
and the class's weekly session windows in `class.json`. A class without them has
`planned_sessions = 0`, so `limit = 0` and the page shows the bare absence count.
 
## The page
 
`view.html` reads `?code=` from the URL, fetches `/reports/<code>.json` and draws:
 
- A title: `<class name> - <class code>`.
- A table with **Nome**, **Matricula** and **FREQ**. FREQ reads `freq/limit` when
  a limit exists, otherwise just `freq`.
- Pagination of **10 students per page**, with **Previous** / **Next** buttons
  (disabled at the first/last page) and a `Page X of Y` label.
## Using it
 
### One-time setup
 
1. Create a Firebase project with **Realtime Database** and **Hosting**.
2. Deploy `view.html` with `firebase deploy --only hosting`.
3. Set the database rules so `/reports` is publicly readable and not publicly
   writable:
```json
{
  "rules": {
    "reports": {
      ".read": true,
      ".write": false
    }
  }
}
```
 
4. Fill in the constants in the firmware:
   - `FIREBASE_HOST` and `FIREBASE_SECRET` in `report_service.cpp`.
   - The hosting URL in `qr_btn_cb` (`scr_export.cpp`).
The device writes with the database secret (`?auth=`), which bypasses the rules;
the page reads without credentials, which is why `.read` must be `true`.
 
### Flow on the device
 
1. **WiFi screen -> Connect to an external network.** Enter the SSID and password
   (the eye button reveals what was typed) and tap **Connect**. The status line
   shows the device IP once connected.
2. **Export screen.** Tap **QR do CSV** on the class card.
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
- **Access is by link only.** There is no expiry or per-professor token: anyone
  who knows the URL, and therefore the class code, can read that class's report.
  The record contains student names and matriculas, so consider this before using
  real rosters (tightening the database rules or adding an expiry field checked
  by the page are the usual next steps).
- **The database secret lives in the firmware.** Do not commit it to a public
  repository.
- **Unknown or never-uploaded code.** The page has nothing to read and stays on
  its loading title.
## Troubleshooting
 
| Symptom | Likely cause |
| --- | --- |
| Log shows `Reason: 201 - NO_AP_FOUND` | The network was not seen in the scan. Check the SSID spelling (case and spaces). The WiFi companion chip is 2.4 GHz only. For an iPhone hotspot, keep the Personal Hotspot screen open and enable **Maximize Compatibility**. |
| Page shows `undefined` and the browser console reports `401` | The database rules block anonymous reads of `/reports`. Publish the rules above. |
| "Falha ao enviar dados" toast | Not connected to the internet, or wrong `FIREBASE_HOST` / `FIREBASE_SECRET`. |
| QR opens a page that never fills in | The class code in the URL has no record yet. Tap **QR do CSV** for that class first. |
 
## Where it lives (for developers)
 
| Piece | File |
| --- | --- |
| Report upload (Firebase `PUT`) | `src/services/report_service.cpp`, `include/services/report_service.h` |
| FREQ computation | `export_compute_freq()` in `src/services/export_service.cpp` |
| External WiFi (STA) | `src/services/wifi_extern.cpp`, `include/services/wifi_extern.h` |
| WiFi screen (SSID/password, Connect) | `src/ui/screens/scr_wifi_editor.cpp` |
| QR button + modal | `src/ui/screens/scr_export.cpp` (`qr_btn_cb`) |
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
void wifi__ip(char* out, size_t cap);

// saves wifi info for next connection
void wifi_restore(void);
bool wifi_get_saved_credentials(char* name, size_t name_cap, char* password, size_t password_cap);
```
 