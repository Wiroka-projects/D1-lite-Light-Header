"""
Auto-Upload Tool for PlatformIO
Watches the firmware binary and uploads to the ESP as soon as a new build appears.
Usage: python auto_upload.py
"""

import os
import sys
import time
import subprocess

# --- Configuration ---
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
ENV_NAME = "d1_mini_lite"
FIRMWARE_PATH = os.path.join(PROJECT_DIR, ".pio", "build", ENV_NAME, "firmware.bin")
POLL_INTERVAL = 1  # seconds between checks


def get_platformio_exe():
    """Find the PlatformIO CLI executable."""
    # Try the typical PlatformIO venv path on Windows
    home = os.path.expanduser("~")
    pio_venv = os.path.join(home, ".platformio", "penv", "Scripts", "platformio.exe")
    if os.path.isfile(pio_venv):
        return pio_venv
    # Fallback: assume it's on PATH
    return "platformio"


def upload_firmware(pio_exe):
    """Run PlatformIO upload."""
    print(f"\n{'='*50}")
    print(f"  New firmware detected — uploading to {ENV_NAME}...")
    print(f"{'='*50}\n")
    result = subprocess.run(
        [pio_exe, "run", "--target", "upload", "-e", ENV_NAME],
        cwd=PROJECT_DIR,
    )
    if result.returncode == 0:
        print(f"\n>>> Upload successful!\n")
    else:
        print(f"\n>>> Upload FAILED (exit code {result.returncode})\n")
    return result.returncode


def main():
    pio_exe = get_platformio_exe()
    
    # Check for --watch-only flag
    watch_only = "--watch-only" in sys.argv
    
    print(f"Auto-Upload watcher started")
    print(f"  Project:   {PROJECT_DIR}")
    print(f"  Env:       {ENV_NAME}")
    print(f"  Watching:  {FIRMWARE_PATH}")
    print(f"  PIO CLI:   {pio_exe}")
    print(f"  Polling every {POLL_INTERVAL}s — press Ctrl+C to stop\n")

    last_mtime = None
    last_size = None

    # Check if firmware exists on startup
    if os.path.isfile(FIRMWARE_PATH):
        stat = os.stat(FIRMWARE_PATH)
        last_mtime = stat.st_mtime
        last_size = stat.st_size
        print(f"  Existing firmware found (mtime={last_mtime:.0f}, size={last_size})")
        
        if watch_only:
            print(f"  [watch-only mode] Will upload on next change.\n")
        else:
            print(f"  Uploading now...\n")
            upload_firmware(pio_exe)
    else:
        print(f"  No firmware yet — waiting for first build...\n")

    try:
        while True:
            time.sleep(POLL_INTERVAL)

            if not os.path.isfile(FIRMWARE_PATH):
                continue

            stat = os.stat(FIRMWARE_PATH)
            mtime = stat.st_mtime
            size = stat.st_size

            if mtime != last_mtime or size != last_size:
                last_mtime = mtime
                last_size = size
                upload_firmware(pio_exe)

    except KeyboardInterrupt:
        print("\nStopped.")
        sys.exit(0)


if __name__ == "__main__":
    main()
