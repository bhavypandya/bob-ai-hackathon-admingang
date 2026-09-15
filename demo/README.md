# Demo

## Demo Video

See [demo-video-link.txt](demo-video-link.txt) for the hosted demo video.

## Demo Flow

The recommended demo sequence for judges:

1. **Start** — Run `src\build\bin\supply_chain_backend.exe --frontend src\frontend`
2. **Open** `http://localhost:8080`
3. **Overview tab** — Show KPI cards: 3+ disruptions, 12+ affected shipments, 8 idle assets
4. **Disruptions tab** — Show Cyclone Biparjoy (CRITICAL) and JNPT Strike (HIGH)
5. **Shipments tab** — Filter by "Critical Risk" — show SH1016 with score 95+
6. **Click SH1016** — Show detail panel: vaccine shipment, CRITICAL priority, cold-chain, reasons
7. **Fleet tab** — Show idle assets T204, T211, T215; redeployment recommendations
8. **Cold Chain tab** — Show SH1016 temperature card (CRITICAL severity)
9. **Click SH1016 in temperature chart** — Show the excursion peak at 15.1°C vs. max 8°C
10. **Simulation tab** — Click "Temperature Excursion" for SH1016
11. **Observe** — Before/after comparison shows cold-chain alert change
12. **Bob AI tab** — Type: "What should we do right now?"
13. **Show Bob's response** — Combined disruption + shipment + fleet + cold-chain action plan

## Screenshots

See the [screenshots/](screenshots/) directory.

## Live Demo

See [live-demo-url.txt](live-demo-url.txt) — local deployment only for this submission.
