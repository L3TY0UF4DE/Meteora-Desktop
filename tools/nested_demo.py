#!/usr/bin/env python3
"""Launch a private Xephyr display and bus, then stop only our child processes."""
import os
from pathlib import Path
import re
import select
import secrets
import subprocess
import sys
import tempfile

stage = Path(sys.argv[1]).resolve()
size = sys.argv[2]
if not re.fullmatch(r'[1-9][0-9]{2,4}x[1-9][0-9]{2,4}', size):
    raise SystemExit('METEORA_DEMO_SIZE must look like 1280x800.')
processes = []
with tempfile.TemporaryDirectory(prefix='meteora-demo-') as work:
    work = Path(work)
    auth = work / 'Xauthority'
    auth.touch(mode=0o600)
    cookie = secrets.token_hex(16)
    # Use a wildcard-family entry so displayfd can safely allocate the display.
    record = 'ffff 0000 0000 0012 ' + 'MIT-MAGIC-COOKIE-1'.encode().hex() + ' 0010 ' + cookie + '\n'
    subprocess.run(['xauth', '-f', str(auth), 'nmerge', '-'], input=record, text=True, check=True)
    reader, writer = os.pipe()
    try:
        server = subprocess.Popen(['Xephyr', '-displayfd', str(writer), '-screen', size,
                                   '-resizeable', '-nolisten', 'tcp', '-auth', str(auth), '-noreset'],
                                  pass_fds=(writer,))
        processes.append(server)
        os.close(writer)
        if not select.select([reader], [], [], 12)[0]:
            raise RuntimeError('Xephyr did not start in time.')
        display = os.read(reader, 32).decode().strip()
        os.close(reader)
        if not display.isdigit():
            raise RuntimeError('Xephyr failed to create a display.')
        env = os.environ.copy()
        env.update(DISPLAY=':' + display, XAUTHORITY=str(auth),
                   XDG_CONFIG_HOME=str(work / 'config'), XDG_CACHE_HOME=str(work / 'cache'),
                   XDG_DATA_DIRS=str(stage / 'share') + ':' + env.get('XDG_DATA_DIRS', '/usr/local/share:/usr/share'),
                   PATH=str(stage / 'bin') + ':' + env['PATH'])
        # Keep installed applications and the real home folder available. Only
        # the nested session's preferences/cache and D-Bus session are isolated.
        session = subprocess.Popen(['dbus-run-session', '--', str(stage / 'bin/meteora-session')], env=env)
        processes.append(session)
        while session.poll() is None and server.poll() is None:
            try:
                session.wait(timeout=0.3)
            except subprocess.TimeoutExpired:
                pass
    except KeyboardInterrupt:
        pass
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
