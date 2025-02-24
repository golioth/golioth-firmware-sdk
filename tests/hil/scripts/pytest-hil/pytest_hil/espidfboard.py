from pytest_hil.board import Board
from pytest_hil.espboard import ESPBoard

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

    async def set_setting(self, key, value):
        await self.send_cmd(f'settings set {key} {value}', wait_str='saved')

    async def reset(self):
        await self.send_cmd('reset', wait_str='Calling app_main*()')

    async def set_wifi_credentials(self, ssid, psk):
        await self.set_setting('wifi/ssid', f'"{ssid}"')
        await self.set_setting('wifi/psk', f'"{psk}"')

    async def set_golioth_psk_credentials(self, psk_id, psk):
        await self.set_setting('golioth/psk-id', f'"{psk_id}"')
        await self.set_setting('golioth/psk', f'"{psk}"')
