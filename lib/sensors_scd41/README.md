
# Project Setup — ot_templatev4

This document explains how to set up, build, and maintain the ot_templatev4 project in a clean and predictable way. It focuses on node-specific overlays, CoAP transport, sensor publishers, and Git hygiene to prevent accidental inclusion of old files or history.

---

0. Maak een nieuwe map bv ot_templatev4



## 🚀 Clean Git Setup (IMPORTANT)



1. Remove any inherited Git history:


rm -rf .git


2. Initialize a fresh Git repo:


git init
git add .
git commit -m "Initial clean v4 commit"
git branch -M main


4. Create a new empty GitHub repo online: https://github.com/new
   - Repository name: `ot_templatev4`
   - Do *not* add README, .gitignore, or license.

5. Connect your local project to GitHub:


git remote add origin git@github.com:stapelaar/ot_templatev4.git
git push -u origin main


Your project is now clean and fully under version control.

---

## 🏗 Building for a Specific Node

Use the `build-xiao.sh` script:

### Build ND12:

./build-xiao.sh --node ND12


This script:
- Creates a per-node build directory (`build/ND12/`)
- Chains extra overlays
- Applies `nodes/NDxx/NDxx.conf` last
- Builds using Zephyr CMake

---


### ✔ Runtime CoAP server configuration

Through the shell:

coap get
coap set <ipv6> [port]


These values are saved via Zephyr settings.

---


## 🔗 Topic Format (MQTT-style)

Topics follow the structure:
```
ND<id>/<CHANNEL>/<DEVICE>/<FIELD>
```

Examples:
```
ND12/OUT/SHT41-1/TEMP
ND12/OUT/SHT41-1/RH
ND12/STATUS/uptime
```

These integrate smoothly with your existing CoAP→MQTT bridge and OpenHAB.

---

## 🔧 Flashing

For Xiao nRF54 boards:
```
./flash-xiao.sh --node ND12
```

---

## 🧪 Testing

On the node shell:

```
ot state
coap get
version
```

Check your broker for:
```
ND12/OUT/SHT41-1/TEMP
ND12/OUT/SHT41-1/RH
ND12/STATUS/uptime
```

---

## 🛡 Git Workflow Recommendations

- Always keep your project **outside** the Zephyr workspace:

```
~/projects/ot_templatev4/
```

- If you accidentally start in the wrong place:

```
rm -rf .git
```

- Then re-init cleanly:
```
git init
git add .
git commit -m "clean start"
```

- Create a new empty GitHub repo (no README), then:
```
git remote add origin git@github.com:stapelaar/ot_templatev4.git
git push -u origin main
```

Your repository will stay clean and predictable.

---

## 🎉 Done!

This project structure is designed for stability, clarity, and long-term maintainability. Each node can be configured independently without code duplication, the transport layer is robust, and your Git workflow is clean and safe.

