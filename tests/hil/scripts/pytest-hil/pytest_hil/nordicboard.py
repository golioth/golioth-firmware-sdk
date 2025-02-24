from pytest_hil.board import Board
from pynrfjprog import HighLevel

class NordicBoard(Board):
    def program(self, fw_image):
        with HighLevel.API() as api:
            with HighLevel.DebugProbe(api, int(self.serial_number)) as probe:
                program_options = HighLevel.ProgramOptions(
                    erase_action=HighLevel.EraseAction.ERASE_ALL,
                    reset=HighLevel.ResetAction.RESET_SYSTEM,
                    verify=HighLevel.VerifyAction.VERIFY_READ,
                )

                probe.program(fw_image, program_options=program_options)
