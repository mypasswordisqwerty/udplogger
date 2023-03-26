#!/usr/bin/env python3

import argparse
import logging
import serial
import os
import json
import sys
import getpass

class Device:
    def __init__(self, dev):
        logging.info(f"Open device {dev}")
        self.f = serial.Serial(dev, 115200, parity=serial.PARITY_NONE,
                               stopbits=serial.STOPBITS_ONE, bytesize=serial.EIGHTBITS)
        resp = self.cmd("ping")

    def cmd(self, cmd):
        logging.debug(f"< {cmd}")
        self.f.write((cmd + "\n").encode('ascii'))
        ln = self.readline(self.f)
        logging.debug(f"> {ln}")
        return ln


    def readline(self, f):
        ret = ""
        while True:
            c = f.read(1)
            ret += c.decode("ascii")
            if c == b'\n':
                return ret

class Config:
    def __init__(self, file=None):
        self.cfg = {}
        if file:
            with open(file) as f:
                self.cfg = json.load(f)

    def parse(self, data):
        p = data.split(':')
        if p[0] != "config":
            raise RuntimeError(f"Not config string: {data}")
        uarts = int(p[3])
        scr = int(p[4])
        ina = int(p[5])
        self.cfg = {
            'ssid': p[1],
            'port': int(p[2]),
            'uarts': [uarts & 0xFF, (uarts >> 8) & 0xFF, (uarts >> 16) & 0xFF],
            'screen': [scr & 0xFF, (scr >> 8) & 0xFF],
            'ina': [ina & 0xFF, (ina >> 8) & 0xFF],
        }

    def print(self):
        return json.dumps(self.cfg, indent=4)

    def dump(self):
        c = self.cfg
        uarts = c['uarts'][0] | (c['uarts'][1] << 8) | (c['uarts'][2] << 16)
        scr = c['screen'][0] | (c['screen'][1] << 8)
        ina = c['ina'][0] | (c['ina'][1] << 8)
        pswd = getpass.getpass("WiFi password: ")
        return f"{c['ssid']}:{pswd}:{c['port']}:{uarts}:{scr}:{ina}"


def find_device():
    devs = [x for x in os.listdir("/dev") if x.startswith("cu.") and 'usbserial' in x]
    if not devs:
        raise RuntimeError("Device not found")
    return os.path.join("/dev", devs[0])

def run_command(dev, opts):
    if opts.command == "read":
        cfg = Config()
        cfg.parse(dev.cmd("getconfig"))
        print(cfg.print())
        return
    if opts.command == "write":
        if not opts.config:
            raise RuntimeError("Config file not specified")
        cfg = Config(opts.config)
        print(dev.cmd("setconfig:" + cfg.dump()))
        return
    resp = dev.cmd(opts.command)
    logging.info(f"Response {resp}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verbose", "-v", action="store_true")
    parser.add_argument("--device", "-d", default=None)
    parser.add_argument("--config", "-c", default=None)
    parser.add_argument("command")
    opts = parser.parse_args()
    logging.basicConfig(level=logging.DEBUG if opts.verbose else logging.INFO, stream=sys.stderr)
    if not opts.device:
        opts.device = find_device()
    dev = Device(opts.device)
    run_command(dev, opts)
    


if __name__ == "__main__":
    main()