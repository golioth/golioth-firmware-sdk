from zephyrboard import ZephyrBoard

class NCSBoard(ZephyrBoard):
    def reset(self):
        self.send_cmd('kernel reboot cold', wait_str='Booting nRF Connect SDK')
