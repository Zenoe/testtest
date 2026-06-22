# Xyre Controller Service

## Architecture

- `Revelation.exe` always runs as the interactive user.
- `XyreController` is an auto-start LocalSystem Windows service hosted by
  `xyreService.exe /controller-service`.
- The app communicates with the controller through
  `\\.\pipe\XyreController.v1`.
- The controller creates, starts, stops, and removes per-tunnel
  `XyGuardTunnel$...` services.
- WireGuard driver access and traffic-counter reads run only inside the
  controller service. The app requests `traffic` by validated adapter alias.
- `xyreService.exe install-controller` is the only operation launched with the
  `runas` verb. It is used only when `XyreController` is not installed.

## Security Boundary

- The pipe rejects remote clients.
- Pipe access is limited to SYSTEM, administrators, and interactive users.
- Interactive users may query and start `XyreController`, but cannot stop,
  reconfigure, or delete it.
- Requests use a versioned JSON protocol and are limited to 64 KiB.
- Client and server pipe operations are cancellable and time bounded.
- A protocol-version mismatch triggers an elevated controller upgrade and restart.

## Deployment Requirements

`xyreService.exe`, `tunnel.dll`, Qt runtime DLLs, and all other service-loaded
dependencies must be installed in a directory that standard users cannot
modify, normally under `Program Files`. Installing a LocalSystem service from a
user-writable directory is a local privilege-escalation vulnerability.

Production packages should code-sign the app, service executable, and DLLs.
Service upgrades should be performed by the signed installer under elevation.
