import sys
import subprocess
import json

def flash_firmware(port):
    with open('build/flasher_args.json', 'r') as f:
        flasher_args = json.load(f)

    esptool_command = [
        "esptool.py",
        "--port", port,
        "--baud", "460800",
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash",
    ]

    for arg in flasher_args['write_flash_args']:
        esptool_command.append(arg)

    for offset, file in flasher_args['flash_files'].items():
        esptool_command.append(offset)
        esptool_command.append("build/{}".format(file))

    print("Starting the flash process. This may take 20-30 seconds to complete.")
    process = subprocess.run(
            esptool_command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True)
    print(process.stdout)
    if process.returncode != 0:
        raise RuntimeError('esptool command failed')

def main():
    if len(sys.argv) != 2:
        print('usage: {} <port>'.format(sys.argv[0]))
        sys.exit(-1)
    port = sys.argv[1]
    flash_firmware(port)

if __name__ == "__main__":
    main()
