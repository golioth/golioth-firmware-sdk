from board import Board
import subprocess

class BMPBoard(Board):
    def program(self, fw_image):
        command = ['gdb',
                   '-ex', "set confirm off",
                   '-ex', "target extended-remote {}".format(self.bmp_port),
                   '-ex', "monitor swdp_scan",
                   '-ex', "attach 1",
                   '-ex', f"load {fw_image}",
                   '-ex', "kill",
                   '-ex', "quit"
                   ]

        subprocess.run(command)
