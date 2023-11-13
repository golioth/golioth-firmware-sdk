import esptool

from board import Board

class ESPBoard(Board):
    def __init__(self, app_offset):
        self.app_offset = str(app_offset)

    def program(self, fw_image):
        cmd = [
            "--port", self.port,
            "--baud", "460800",
            "write_flash",
            "-e",
            self.app_offset,
            fw_image,
        ]

        esptool.main(cmd)
