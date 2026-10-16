import logging
import shlex
import subprocess

from pytest_hil.board import Board

LOGGER = logging.getLogger(__name__)


class NordicBoard(Board):
    def program(self, fw_image):
        command = [
            'nrfutil', 'device', 'program',
            '--firmware', fw_image,
            '--serial-number', self.serial_number,
            '--options', 'chip_erase_mode=ERASE_ALL,reset=RESET_SYSTEM,verify=VERIFY_READ',
        ]

        LOGGER.info('Running: %s', shlex.join(command))
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True, check=False)
        LOGGER.info('nrfutil exited with code %d, output:\n%s', result.returncode,
                    result.stdout.rstrip())

        if result.returncode != 0:
            raise RuntimeError(f'nrfutil exited with code {result.returncode}: '
                               f'{shlex.join(command)}\n{result.stdout}')
