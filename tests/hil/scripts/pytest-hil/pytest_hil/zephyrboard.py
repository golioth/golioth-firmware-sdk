from pytest_hil.board import Board

class  ZephyrBoard(Board):
    @property
    def PROMPT(self):
        return 'uart:'

    async def set_setting(self, key, value):
        await self.send_cmd(f'settings set {key} {value}', wait_str='saved')

    async def reset(self):
        await self.send_cmd('kernel reboot cold', wait_str='Booting Zephyr OS')

    async def set_wifi_credentials(self, ssid, psk):
        await self.send_cmd(f'wifi cred add -k 1 -s "{ssid}" -p "{psk}"')
        await self.send_cmd('wifi cred auto_connect')

    async def set_golioth_psk_credentials(self, psk_id, psk):
        await self.set_setting('golioth/psk-id', f'"{psk_id}"')
        await self.set_setting('golioth/psk', f'"{psk}"')
