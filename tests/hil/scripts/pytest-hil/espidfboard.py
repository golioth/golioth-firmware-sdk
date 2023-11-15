from board import Board
from espboard import ESPBoard

class ESPIDFBoard(ESPBoard):
    def __init__(self, *args, **kwargs):
        ESPBoard.__init__(self, 0x0)
        Board.__init__(self, *args, **kwargs)

    @property
    def PROMPT(self):
        return 'esp32>'

    @property
    def USES_WIFI(self):
        return True

    def set_setting(self, key, value):
        self.send_cmd(f'settings set {key} {value}', wait_str='saved')

    def reset(self):
        self.send_cmd('reset', wait_str='Calling app_main*()')

    def set_wifi_credentials(self, ssid, psk):
        self.set_setting('wifi/ssid', f'"{ssid}"')
        self.set_setting('wifi/psk', f'"{psk}"')

    def set_golioth_psk_credentials(self, psk_id, psk):
        self.set_setting('golioth/psk-id', f'"{psk_id}"')
        self.set_setting('golioth/psk', f'"{psk}"')
