import atexit
import re
import select
import subprocess
from time import time

from board import Board

class LinuxBoard(Board):
    def __init__(self, fw_image):
        self.binary = fw_image
        self.rcvd_lines = []

    def wait_for_regex_in_line(self, regex, timeout_s=20, log=True):
        start_time = time()
        while True:
            # Check for timeout
            if time() - start_time > timeout_s:
                raise RuntimeError('Timeout')

            # Read input. We use communicate() because the other read
            # read functions for pipes don't have timeouts, and poll()
            # doesn't tell us how many lines (or bytes) are available to
            # read. communicate() runs until the program terminates, so
            # we use a short timeout to prevent blocking. The output
            # received during the call is contained in the exception.
            try:
                self.process.communicate(timeout=0.1)
            except subprocess.TimeoutExpired as e:
                if e.output == None:
                    continue
                lines = e.output.decode("UTF-8")

            for line in lines.splitlines():
                # When communicate() times out, it leaves the data intact
                # in the pipe, so we need to do our own filtering for
                # repeated lines
                if line in self.rcvd_lines:
                    continue
                self.rcvd_lines.append(line)

                if line != "" and log:
                    print(f"time: {time()} log: {line}")

                regex_search=re.search(regex, line)
                if regex_search:
                    return regex_search

    @property
    def PROMPT(self):
        return ''

    @property
    def USES_WIFI(self):
        return False

    def set_setting(self, key, value):
        pass

    def reset(self):
        env = {
            "GOLIOTH_PSK_ID" : self.golioth_psk_id,
            "GOLIOTH_PSK" : self.golioth_psk
        }

        self.process = subprocess.Popen(["stdbuf", "-o0", self.binary],
                                        stdout=subprocess.PIPE,
                                        bufsize=1,
                                        universal_newlines=True,
                                        env=env)

        # Register atexit() handler to kill test program
        atexit.register(self.process.terminate)

    def program(self):
        pass

    def set_wifi_credentials(self, ssid, psk):
        pass

    def set_golioth_psk_credentials(self, psk_id, psk):
        self.golioth_psk_id = psk_id
        self.golioth_psk = psk
