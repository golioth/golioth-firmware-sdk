from pytest_hil.board import Board
from pytest_hil.zephyrboard import ZephyrBoard
import subprocess
from time import sleep

class PICO2(ZephyrBoard, Board):
    def program(self, fw_image):
        openocd_bin = self.openocd_binary if self.openocd_binary is not None else 'openocd'
        openocd_script = self.openocd_script_dir

        command = [f'{openocd_bin}',
                   '-c', f'adapter serial {self.serial_number}',
                   '-c', 'source [find interface/cmsis-dap.cfg]',
                   '-c', 'source [find target/rp2350.cfg]',
                   '-c', 'adapter speed 5000',
                   '-c', 'init',
                   '-c', 'targets',
                   '-c', 'reset init',
                   '-c', 'flash erase_sector 0 0 last',
                   '-c', f'program {fw_image} reset exit'
                   ]

        if self.openocd_script_dir is not None:
            command += [ '-s', f'{self.openocd_script_dir}']

        print("######################################################")
        print(command)
        print("######################################################")
        subprocess.run(command)

    @property
    def USES_WIFI(self):
        return True
