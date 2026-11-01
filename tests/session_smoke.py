#!/usr/bin/env python3
"""Exercise a real X11/Openbox/Meteora session in an isolated virtual display.
Requires Xvfb, xdotool, xprop, xwininfo, xterm and dbus-run-session.
Run: dbus-run-session -- python3 tests/session_smoke.py --build build
"""
import argparse
import json
import os
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument('--build', default='build')
parser.add_argument('--size', default='1280x800')
parser.add_argument('--output', default='test-output')
args = parser.parse_args()
build = Path(args.build).resolve()
output = Path(args.output).resolve()
output.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parents[1]
processes = []
logs = []
checks = []

def check(condition, label):
    if not condition:
        raise AssertionError(label)
    checks.append(label)
    print('PASS:', label, flush=True)

def run(*cmd, timeout=8, **kwargs):
    return subprocess.run([str(c) for c in cmd], env=env, check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True, timeout=timeout, **kwargs).stdout.strip()

def spawn(name, cmd, **kwargs):
    log = (output / (name + '.log')).open('w')
    logs.append(log)
    p = subprocess.Popen([str(c) for c in cmd], env=env, stdout=log,
                         stderr=subprocess.STDOUT, start_new_session=True, **kwargs)
    processes.append(p)
    return p

def until(predicate, message, timeout=8):
    end = time.monotonic() + timeout
    last = None
    while time.monotonic() < end:
        try:
            value = predicate()
            if value:
                return value
        except (subprocess.CalledProcessError, json.JSONDecodeError, KeyError) as exc:
            last = exc
        time.sleep(0.08)
    raise AssertionError(f'Timed out: {message}; {last}')

def shell(*options):
    return json.loads(run(build / 'meteora-shell', *options))

def status():
    return shell('--status')['status']

def key(keys):
    run('xdotool', 'key', '--clearmodifiers', keys)

def window(title):
    return next((w for w in status()['windows'] if w['title'] == title), None)

def click_rect(rect):
    x, y, w, h = rect
    run('xdotool', 'mousemove', str(x + w // 2), str(y + h // 2), 'click', '1')

def metrics(pid):
    values = {}
    for line in Path(f'/proc/{pid}/smaps_rollup').read_text().splitlines():
        if ':' in line:
            k, val = line.split(':', 1)
            if k in ('Rss', 'Pss', 'Private_Clean', 'Private_Dirty'):
                values[k + '_KiB'] = int(val.split()[0])
    stat = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
    values['cpu_ticks'] = int(stat[11]) + int(stat[12])
    return values

with tempfile.TemporaryDirectory(prefix='meteora-session-test-') as temp:
    temp = Path(temp)
    env = os.environ.copy()
    env.update(HOME=str(temp), XDG_CONFIG_HOME=str(temp / 'config'),
               XDG_DATA_HOME=str(temp / 'data'), XDG_CACHE_HOME=str(temp / 'cache'),
               XDG_DATA_DIRS=str(build / 'stage/share') + ':/usr/local/share:/usr/share',
               XDG_CURRENT_DESKTOP='Meteora', QT_QPA_PLATFORM='xcb', QT_SCALE_FACTOR='1',
               QT_AUTO_SCREEN_SCALE_FACTOR='0', QT_FONT_DPI='96',
               PATH=str(build) + ':' + os.environ['PATH'])
    runtime = temp / 'runtime'
    runtime.mkdir(mode=0o700)
    env['XDG_RUNTIME_DIR'] = str(runtime)
    appdir = temp / 'data/applications'
    appdir.mkdir(parents=True)
    # A genuine, harmless test application: writes a sentinel then exits.
    marker = temp / 'launched'
    script = temp / 'launch-fixture'
    script.write_text(f'#!/bin/sh\nprintf launched > "{marker}"\n')
    script.chmod(0o700)
    (appdir / 'meteora-test.desktop').write_text(
        f'[Desktop Entry]\nType=Application\nName=Meteora Launch Probe\n'
        f'Exec="{script}"\nCategories=Utility;\nIcon=system-run\n')
    try:
        readfd, writefd = os.pipe()
        xserver = spawn('xvfb', ['Xvfb', '-displayfd', str(writefd), '-screen', '0', args.size+'x24',
                                 '-nolisten', 'tcp', '-noreset'], pass_fds=(writefd,))
        os.close(writefd)
        ready, _, _ = select.select([readfd], [], [], 10)
        check(bool(ready), 'virtual display started')
        env['DISPLAY'] = ':' + os.read(readfd, 32).decode().strip()
        os.close(readfd)
        wm = spawn('openbox', ['openbox', '--config-file', root / 'session/openbox.xml'])
        until(lambda: '0x' in run('xprop', '-root', '_NET_SUPPORTING_WM_CHECK'), 'window manager startup')
        started = time.monotonic()
        desktop = spawn('shell', [build / 'meteora-shell'])
        initial = until(status, 'shell startup')
        ready_seconds = time.monotonic() - started
        check(initial['desktopCount'] == 2, 'two real workspaces')
        check(initial['notificationServer'], 'notification D-Bus service registered')
        panel_id = initial['panelWindow']
        check('_NET_WM_WINDOW_TYPE_DOCK' in run('xprop', '-id', panel_id, '_NET_WM_WINDOW_TYPE'), 'panel is a real dock')
        work = run('xprop', '-root', '_NET_WORKAREA')
        check('_NET_WORKAREA(CARDINAL)' in work, 'window manager publishes reserved work area')
        terminal = spawn('terminal', ['xterm', '-title', 'Meteora Demo Terminal', '-fa', 'DejaVu Sans Mono',
                                     '-fs', '10', '-bg', '#0e1723', '-fg', '#dceeff', '-geometry', '62x18+530+75'])
        term = until(lambda: window('Meteora Demo Terminal'), 'real application managed')
        wid = term['id']
        check(wid in status()['taskButtons'], 'real window appears in taskbar')
        run('xdotool', 'windowactivate', '--sync', wid)
        key('Super_L')
        until(lambda: status()['launcherVisible'], 'Super opens launcher above focused app')
        run('xdotool', 'type', '--clearmodifiers', 'Meteora Launch Probe')
        until(lambda: status()['visibleApplications'] == ['Meteora Launch Probe'], 'search filters installed apps')
        key('Return')
        until(marker.exists, 'Enter launches actual desktop entry')
        check(not status()['launcherVisible'], 'launcher closes after successful activation')
        key('Super_L')
        until(lambda: status()['launcherVisible'], 'launcher reopens')
        key('Escape')
        until(lambda: not status()['launcherVisible'], 'Escape dismisses launcher')
        check(True, 'global Super, keyboard search, Enter and Escape')
        run('xdotool', 'windowactivate', '--sync', wid)
        key('super+2')
        until(lambda: status()['desktop'] == 1, 'workspace shortcut')
        check(not status()['launcherVisible'], 'Super chord does not open launcher')
        check(wid not in status()['taskButtons'], 'taskbar filters windows by workspace')
        key('super+1')
        until(lambda: status()['desktop'] == 0 and wid in status()['taskButtons'], 'return to workspace')
        click_rect(status()['taskButtons'][wid])
        until(lambda: window('Meteora Demo Terminal')['minimized'], 'taskbar minimizes active app')
        click_rect(status()['taskButtons'][wid])
        until(lambda: not window('Meteora Demo Terminal')['minimized'], 'taskbar restores app')
        check(True, 'taskbar minimize and restore')
        key('super+Up')
        time.sleep(0.15)
        geom = dict(line.split('=', 1) for line in run('xdotool', 'getwindowgeometry', '--shell', wid).splitlines() if '=' in line)
        check(int(geom['Y']) + int(geom['HEIGHT']) <= status()['panelRect'][1], 'maximized window does not cover panel')
        check(not status()['launcherVisible'], 'maximize shortcut does not toggle launcher')
        key('super+Down')
        shell('--settings')
        settings = until(lambda: window('Meteora Settings'), 'settings window launched')
        check(settings['id'] in status()['taskButtons'], 'settings managed as a normal window')
        run('xdotool', 'windowactivate', '--sync', settings['id'])
        key('alt+Tab')
        active = run('xdotool', 'getactivewindow')
        check(active == wid, 'Alt+Tab switches between real applications')
        # Settings changes are external atomic writes, just like QSettings.
        config = Path(status()['preferences'])
        config.parent.mkdir(parents=True, exist_ok=True)
        replacement = config.with_suffix('.new')
        replacement.write_text('[panel]\nheight=56\nfloating=false\n[appearance]\nplain=true\n')
        replacement.replace(config)
        until(lambda: status()['panelRect'][3] == 56 and status()['panelRect'][0] == 0, 'live settings update')
        check(True, 'atomic settings changes update the live panel without polling')
        replacement.write_text('[panel]\nheight=50\nfloating=true\n[appearance]\nplain=false\n[notifications]\ndnd=false\n')
        replacement.replace(config)
        until(lambda: status()['panelRect'][3] == 50, 'restore defaults')
        notice_args = ('gdbus', 'call', '--session', '--dest', 'org.freedesktop.Notifications',
                       '--object-path', '/org/freedesktop/Notifications', '--method')
        notify = lambda title: run(*notice_args, 'org.freedesktop.Notifications.Notify', 'Meteora test',
                                   '0', '', title, 'Real application notifications are working.', '[]', '{}', '5000')
        notify('Ready when you are.')
        until(lambda: status()['toastVisible'] and status()['notifications'] == 1, 'notification popup')
        replacement.write_text('[notifications]\ndnd=true\n')
        replacement.replace(config)
        until(lambda: not status()['toastVisible'], 'DND hides popup')
        notify('Quiet notification')
        until(lambda: status()['notifications'] == 2, 'DND retains history')
        check(not status()['toastVisible'], 'Do Not Disturb suppresses popups, keeps history')
        replacement.write_text('[notifications]\ndnd=false\n')
        replacement.replace(config)
        # Warm caches, then prove opening/closing the launcher does not leak each time.
        for _ in range(4): shell('--toggle-launcher')
        before = metrics(desktop.pid)
        for _ in range(40): shell('--toggle-launcher')
        time.sleep(0.3)
        after = metrics(desktop.pid)
        check(after['Pss_KiB'] - before['Pss_KiB'] < 8192, '40 launcher toggles grow PSS by less than 8 MiB')
        run('xdotool', 'windowactivate', '--sync', settings['id'])
        run('xdotool', 'windowmove', settings['id'], '520', '90')
        shell('--capture', output / 'desktop-1280.png')
        shell('--toggle-launcher')
        until(lambda: status()['launcherVisible'], 'launcher for capture')
        time.sleep(0.15)
        shell('--capture', output / 'launcher-1280.png')
        shell('--toggle-launcher')
        # End the test apps before measuring desktop idle resource use.
        run('xdotool', 'windowactivate', '--sync', settings['id'])
        key('alt+F4')
        until(lambda: not window('Meteora Settings'), 'Alt+F4 closes settings')
        terminal.terminate()
        terminal.wait(timeout=5)
        until(lambda: not window('Meteora Demo Terminal'), 'task removed when app exits')
        check(not status()['taskButtons'], 'closed apps disappear from taskbar')
        time.sleep(1)
        base = {name: metrics(p.pid) for name, p in [('shell', desktop), ('openbox', wm), ('xserver', xserver)]}
        t0 = time.monotonic()
        time.sleep(10)
        elapsed = time.monotonic()-t0
        final = {name: metrics(p.pid) for name, p in [('shell', desktop), ('openbox', wm), ('xserver', xserver)]}
        hz = os.sysconf('SC_CLK_TCK')
        for name in final:
            final[name]['idle_cpu_percent_one_core'] = round((final[name]['cpu_ticks']-base[name]['cpu_ticks'])/hz/elapsed*100, 3)
        check(final['shell']['idle_cpu_percent_one_core'] < 1, 'shell idle CPU below 1% of one test-host core')
        shell('--capture', output / 'clean-desktop.png')
        report = {'checks': checks, 'resolution': args.size, 'ready_seconds': round(ready_seconds,3),
                  'resources': final, 'launcher_pss_growth_KiB': after['Pss_KiB']-before['Pss_KiB'],
                  'notes': 'Release build, Xvfb software X11, Ubuntu 24.04 test container. Not a bare-metal whole-system RAM measurement.'}
        (output / 'session-report.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps(report, indent=2), flush=True)
        shell('--quit')
        check(desktop.wait(timeout=5) == 0, 'clean shell shutdown')
    finally:
        for p in reversed(processes):
            if p.poll() is None:
                p.terminate()
                try:
                    p.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait()
        for log in logs:
            log.close()
