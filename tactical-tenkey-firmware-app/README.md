# Firmware Update App

Browser-based firmware update portal for the Tactical TenKey. Uses [ESP Web Tools](https://esphome.github.io/esp-web-tools/) for flashing over USB. Hosted on Cloudflare Pages.

## How It Works

The web app fetches the latest release from the GitHub Releases API at runtime and builds an ESP Web Tools manifest pointing at the release asset. No firmware files are stored in the repo.

## Updating Firmware

1. Bump `version.json` in the repo root and push to `main`
2. The `release.yml` GitHub Action builds the firmware and creates a new GitHub Release with `firmware.bin` attached
3. The web app picks up the new release automatically

## Deployment

Hosted on Cloudflare Pages, pointed at this repo with `t2-firmware-app/public` as the output directory. Redeploys automatically on every push to `main`.

## Project Structure

```
t2-firmware-app/
├── public/
│   └── index.html
└── README.md
```

## Requirements

- Chromium-based browser for flashing (Chrome, Edge, Brave). Some Firefox versions may also work
- USB-C data cable connected to Tactical Tenkey
