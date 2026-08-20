# This file is part of KDE
# SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>
#
# SPDX-License-Identifier: LGPL-2.0-or-later

"""Starts and stops the server the http worker tests talk to.

ctest runs this once around the tests, as a fixture, so that they do not each have to bring a
server up of their own and so that one is running before the first of them asks for a page.
"""

import os
import signal
import socket
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))


def listening(port):
    with socket.socket() as probe:
        return probe.connect_ex(("127.0.0.1", port)) == 0


def start(port, pidfile):
    if listening(port):
        print(f"something is already listening on port {port}", file=sys.stderr)
        return 1

    # Whatever starts this waits for the pipes it handed over to close, so the server is given a
    # log of its own to write to rather than the ones it inherited. Otherwise the caller waits for
    # a server that is not going to stop.
    log = open(os.path.join(os.path.dirname(pidfile), "httpserver.log"), "w")
    server = subprocess.Popen(
        [sys.executable, os.path.join(HERE, "httpserver.py"), str(port)],
        start_new_session=True,
        stdin=subprocess.DEVNULL,
        stdout=log,
        stderr=subprocess.STDOUT,
    )

    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if listening(port):
            with open(pidfile, "w") as f:
                f.write(str(server.pid))
            return 0
        if server.poll() is not None:
            print(f"the server exited with {server.returncode} before it listened", file=sys.stderr)
            return 1
        time.sleep(0.1)

    server.terminate()
    print(f"the server did not listen on port {port} within 30 seconds", file=sys.stderr)
    return 1


def stop(pidfile):
    try:
        with open(pidfile) as f:
            pid = int(f.read())
    except (OSError, ValueError):
        # Nothing to stop, which is what the caller wants anyway.
        return 0

    os.unlink(pidfile)
    try:
        # The server was given a session of its own, so this reaches the flask reloader too.
        os.killpg(os.getpgid(pid), signal.SIGTERM)
    except (ProcessLookupError, PermissionError):
        pass
    return 0


if __name__ == "__main__":
    if sys.argv[1] == "start":
        sys.exit(start(int(sys.argv[2]), sys.argv[3]))
    sys.exit(stop(sys.argv[2]))
