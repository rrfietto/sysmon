import fcntl
import os
from pathlib import Path
import pty
import re
import select
import signal
import struct
import subprocess
import termios
import time
import unittest


class DashboardTest(unittest.TestCase):
    def test_resize_themes_and_cleanup(self):
        for shutdown in ('q', signal.SIGINT, signal.SIGTERM):
            with self.subTest(shutdown=shutdown):
                master, slave = pty.openpty()
                original = termios.tcgetattr(slave)
                process = None
                try:
                    def resize(rows, cols):
                        fcntl.ioctl(slave, termios.TIOCSWINSZ,
                                    struct.pack('HHHH', rows, cols, 0, 0))
                        if process is not None:
                            process.send_signal(signal.SIGWINCH)

                    def output(delay=0.35):
                        chunks = []
                        deadline = time.monotonic() + delay
                        while time.monotonic() < deadline:
                            if select.select([master], [], [], 0.02)[0]:
                                chunks.append(os.read(master, 65536))
                        return b''.join(chunks).decode('utf-8', errors='replace')

                    def check_frame(text, rows, cols, small=False):
                        self.assertIn('\x1b[H', text)
                        frame = text.split('\x1b[H')[-1]
                        self.assertNotIn('\n', frame)
                        positions = re.findall(r'\x1b\[(\d+);1H', frame)
                        self.assertTrue(positions)
                        self.assertLessEqual(max(map(int, positions)), rows)
                        for line in re.split(r'\x1b\[\d+;1H', frame)[1:]:
                            plain = re.sub(r'\x1b\[[0-9;]*[mKJ]', '', line)
                            self.assertLessEqual(len(plain), cols - 1)
                        if small:
                            self.assertIn('Window too small', frame)
                        else:
                            self.assertEqual(frame.count('[CPU]'), 1)
                            self.assertEqual(frame.count('[PROCESSES]'), 1)

                    resize(24, 80)
                    process = subprocess.Popen(
                        [str(Path(__file__).resolve().parents[1] / 'sysmon')],
                        stdin=slave, stdout=slave, stderr=slave)
                    check_frame(output(0.6), 24, 80)
                    for rows, cols in ((14, 50), (40, 120), (8, 35), (24, 80)):
                        resize(rows, cols)
                        check_frame(output(), rows, cols, rows < 14)
                    os.write(master, b't')
                    check_frame(output(), 24, 80)
                    os.write(master, b't')
                    mono = output()
                    check_frame(mono, 24, 80)
                    self.assertNotRegex(mono, r'\x1b\[(?:1;)?3[0-9]m')
                    os.write(master, b'\x1b[B\x1b[6~mcpt')
                    check_frame(output(), 24, 80)
                    if shutdown == 'q':
                        os.write(master, b'q')
                    else:
                        process.send_signal(shutdown)
                    process.wait(timeout=3)
                    self.assertEqual(process.returncode,
                                     0 if shutdown == 'q' else 128 + shutdown)
                    self.assertIn('\x1b[?1049l', output(0.1))
                    self.assertEqual(termios.tcgetattr(slave), original)
                finally:
                    if process is not None and process.poll() is None:
                        process.kill()
                        process.wait()
                    os.close(master)
                    os.close(slave)


if __name__ == '__main__':
    unittest.main()
