from board import Board

class  ZephyrBoard(Board):
    @property
    def PROMPT(self):
        return 'uart:'

    def set_setting(self, key, value):
        self.send_cmd(f'settings set {key} {value}', wait_str='saved')

    def reset(self):
        self.send_cmd('kernel reboot cold', wait_str='Booting Zephyr OS')

    def set_wifi_credentials(self, ssid, psk):
        self.set_setting('wifi/ssid', f'"{ssid}"')
        self.set_setting('wifi/psk', f'"{psk}"')

    def set_golioth_psk_credentials(self, psk_id, psk):
        self.set_setting('golioth/psk-id', f'"{psk_id}"')
        self.set_setting('golioth/psk', f'"{psk}"')
