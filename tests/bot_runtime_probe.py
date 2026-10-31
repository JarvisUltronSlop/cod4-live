"""Manual, destructive-to-the-match Bot Warfare acceptance test on a local server.

Start an isolated server with the desired ui_maxclients/sv_maxclients capacity,
then explicitly run this probe. It changes bot settings and restarts the match.
It is deliberately not a CTest test and uses only loopback UDP.
"""
import argparse
import json
import re
import socket
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--rcon-password", required=True)
    parser.add_argument("--capacity", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--console-log", type=Path,
                        help="Optional isolated server log; reject new script errors during this run")
    args = parser.parse_args()
    if not 3 <= args.capacity <= 64:
        parser.error("capacity must be 3..64")
    if any(char.isspace() for char in args.rcon_password):
        parser.error("use a test RCON password without whitespace")
    address = ("127.0.0.1", args.port)
    evidence = {}
    log_offset = args.console_log.stat().st_size if args.console_log and args.console_log.exists() else 0

    def request(command):
        # A fresh socket isolates multipart RCON output from the next query.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(5.0)
            sock.sendto(b"\xff" * 4 + command.encode("ascii"), address)
            parts = []
            try:
                while True:
                    parts.append(sock.recv(16384).decode("latin1"))
                    if command == "getstatus":
                        break
                    sock.settimeout(0.25)
            except (TimeoutError, ConnectionResetError):
                pass
            return "".join(parts)

    def rcon(command):
        time.sleep(0.6)  # The engine rate-limits RCON packets.
        return request("rcon " + args.rcon_password + " " + command)

    def count(expected):
        deadline = time.monotonic() + 45
        body = ""
        while time.monotonic() < deadline:
            body = request("getstatus")
            if body.startswith("\xff" * 4 + "statusResponse\n"):
                players = [line for line in body.splitlines()[2:] if line]
                if len(players) == expected:
                    return body
            time.sleep(0.3)
        raise AssertionError(f"Expected {expected} bots; last status: {body}")

    def movement():
        deadline = time.monotonic() + 15
        origins = []
        while time.monotonic() < deadline:
            first = rcon("live_botStatus")
            origins = re.findall(r"bot (\d+).*?origin=\(([^)]+)\)", first)
            if origins:
                break
        assert origins, "No active bot diagnostics"
        baseline = dict(origins)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            time.sleep(1)
            second = rcon("live_botStatus")
            current = dict(re.findall(r"bot (\d+).*?origin=\(([^)]+)\)", second))
            if any(current.get(index) not in (None, origin) for index, origin in baseline.items()):
                return {"before": first, "after": second}
        raise AssertionError("Bots did not move within 20 seconds")

    def restart(command):
        # Restart commands are queued. An immediate getstatus may still describe
        # the old match, so wait for the native server generation to change.
        previous = re.search(r'"sv_serverid" is: "(\d+)', rcon("sv_serverid"))
        assert previous, "Could not read server generation"
        rcon(command)
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            current = re.search(r'"sv_serverid" is: "(\d+)', rcon("sv_serverid"))
            if current and current.group(1) != previous.group(1):
                return
        raise AssertionError(f"{command} did not start a new server generation")

    try:
        rcon("set live_botCount 2")
        evidence["initial"] = count(2)
        evidence["movement"] = movement()
        rcon("set live_botCount 63")
        evidence["reserved_player_slot"] = count(args.capacity - 1)
        rcon("set live_botDifficulty 7")
        evidence["difficulty"] = rcon("bots_skill")
        assert '"7^7"' in evidence["difficulty"], "Difficulty did not reach upstream bots_skill"
        restart("fast_restart")
        evidence["fast_restart_count"] = count(args.capacity - 1)
        evidence["fast_restart_movement"] = movement()
        restart("map_restart")
        evidence["full_restart_count"] = count(args.capacity - 1)
        evidence["full_restart_movement"] = movement()
        rcon("set live_botCount 1")
        evidence["decrease"] = count(1)
        rcon("set live_botCount 0")
        evidence["disabled"] = count(0)
        rcon("set live_botCount 2")
        evidence["readded"] = count(2)
        evidence["readded_movement"] = movement()
        if args.console_log:
            raw = args.console_log.read_bytes()
            new_log = raw[log_offset if len(raw) >= log_offset else 0:].decode("utf-8", errors="replace")
            evidence["script_errors"] = [line for line in new_log.splitlines()
                                         if "script runtime error" in line or "script compile error" in line]
            assert not evidence["script_errors"], "Server reported script errors during acceptance"
        evidence["passed"] = True
    finally:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    print(f"Bot Warfare acceptance passed for {args.capacity} slots; evidence: {args.output}")


if __name__ == "__main__":
    main()
