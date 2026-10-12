# nixtop crash investigation — 2026-10-10

## Machine
- System76 Pangolin (pang14), AMD Ryzen 7 7840U, 32 GB RAM
- BIOS: `APX928_V00.01_T0008ASY1_ms`, dated **09/27/2023** (American Megatrends)
- NixOS 26.11, kernel 6.18.55, journald persistent (1.1 GB, verifies clean)
- **Motherboard was recently swapped** (per user)

## Symptom: repeated abrupt power-offs (no shutdown, no panic)
Boot history around the failures:

| boot | window | uptime | gap after death |
|---|---|---|---|
| -9 | Oct 8 17:15→17:25 | 10m | 1m22s |
| -8 | Oct 8 17:26→17:29 | 3m | 1m06s |
| -7 | Oct 8 17:30→17:45 | 15m | 17s |
| -6 | Oct 8 17:45→18:36 | 51m | 15s |
| -5 | Oct 8 18:36→Oct 9 07:35 | 13h | 30s |
| -4 | Oct 9 07:35→08:23 | 48m | 13s |
| -3 | Oct 9 08:23→16:39 | 8h16m | 26s |
| -2 | Oct 9 16:40→Oct 10 07:55 | 15h15m | **27m (off/on uncertain)** |
| -1 | Oct 10 08:22→08:49 | 28m | 19s |
| 0 | Oct 10 08:50→ | running | |

## What it is NOT (all verified)
- **No kernel panic / oops / BUG** in any boot's journal — log ends mid-write during
  routine activity (a container healthcheck, an SSH session, etc.)
- **No OOM kill** (32 GB, 4.4 GB used at crash time)
- **No thermal trip** (35 °C at boot; no throttle/critical events)
- **No MCE/EDAC hardware errors** (none logged)
- **No pstore crash dump** (`/sys/fs/pstore` empty after every crash — consistent with
  power cut rather than kernel panic, since panic would write to pstore/efi)
- **Not filesystem corruption** — `journalctl --verify` passes 100%
- **RTC survives** — every boot's RTC readout matches wall clock; not a dead CMOS reset
- **v4l2loopback taint (4096)** — benign out-of-tree module, unrelated

## What it IS consistent with
**Hardware power delivery / EC / board-level issue:**
- The machine powers off instantly (journal stops mid-write; kernel doesn't even have
  time to log a panic — or power is cut before flash commit)
- Auto-restart 13–30 s later = EC or BIOS auto-repower behavior
- **Battery telemetry is broken**: status says "Charging" forever, stuck at 99%,
  `power_now=0`, `energy_now` frozen — the EC/battery interface reports bogus data
- ACPI BIOS errors on every boot: duplicate `_RMV` objects for `RTL8` (Realtek NIC
  power-removal), `TPNL`/`TPDD` (touchpad) — `AE_ALREADY_EXISTS`, symptomatic of the
  board's DSDT mismatching what the kernel expects (or a repatched/clone board)
- Crashes began Oct 8 evening (first short 3-10min boots at 17:15-17:30)

## Recommendations (in order)
1. **BIOS update** — 09/27/2023 firmware on a swapped board is the top suspect.
   System76 Pangolin firmware via `fwupd` or the System76 Firmware Manager
   (requires the System76 Open Firmware / `system76-firmware` package, boot the
   firmware updater USB). Also check the board revision matches pang14.
2. **Reseat/inspect power path** — AC jack, DC-in cable, battery connector on the new
   board. A marginal barrel/dcin connection cuts power exactly like this.
3. **Run with battery disconnected (AC only) for a day** — if the bogus battery
   telemetry (stuck 99%, power_now=0) is feeding the EC wrong data, an EC-triggered
   protection shutdown is plausible. If crashes stop on AC-only → battery/EC issue.
4. **Check the EC firmware version** — a swapped board usually carries the EC of
   whatever board was installed; a mismatched EC firmware on a pang14 is known to
   cause exactly these phantom "Charging"/wrong power readings.
5. **Memtest** — 10 min of memtest86+ rules out the swapped board's RAM slot seating.
6. Kernel-side mitigation (nothing to fix — but for evidence): enable
   `pstore/ramoops` or `efi_pstore` with a kdump so the *next* death leaves a trace.

## OS-level loose ends found while investigating (separate from the crash)
- sshd was hardened in the Oct-8 NixOS rebuild: `AllowTcpForwarding no` → broke all
  SSH tunnels (port-forwarding). Fix when convenient:
  `services.openssh.settings.AllowTcpForwarding = true;` in
  `~/nix-config-new/hosts/nixtop/configuration.nix` (or wherever sshd is configured)
- NixOS firewall allows only 22/5353/41641 — port 4096 (OpenCode server) blocked
- `mongo-express` container crash-loops (restartCount 100+, `mongo` dep not up)
- `wazuh-agent-auth.service` failed
- OpenCode 1.18.33 installed + working with Tiberius GLM-5.3-flash
  (key at `~/.config/opencode/tiberius.key`; auth.json written; env
  `TIBERIUS_LLM_KEY` is what the config's `{env:...}` expects)
