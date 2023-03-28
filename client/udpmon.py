#!/usr/bin/env python3

import argparse
import logging
import sys
import socket
import psutil

logger = logging.getLogger()


def getNetsBroadcast():
    ret = []
    for x,y in psutil.net_if_addrs().items():
        ips = [z.broadcast for z in y if z.family == socket.AF_INET and z.broadcast]
        if ips:
            ret += ips
    logger.debug(f"Found broadcasts {ret}")
    return ret


def broadcastPing(opts):
    hosts = [opts.host] if opts.host else getNetsBroadcast()
    if not hosts:
        raise Exception("Networks for broadcasts not found")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.settimeout(3.0)
    for x in hosts:
        logger.debug(f"Send ping broadcast to {x}:{opts.port}")
        try:
            sock.sendto(b"UUL PING", (x, opts.port))
            data, server = sock.recvfrom(512)
            logger.debug(f"Got response from {server}: {data}")
            if data == b"UUL PONG":
                logger.debug(f"Server discovered at {server}")
                return server[0]
        except socket.timeout:
            logger.debug(f"No answer from {x}")
    sock.close()
    return None


def run(opts, args):
    level = logging.INFO if opts.verbose < 1 else logging.DEBUG
    logging.basicConfig(level=level, format='%(asctime)s.%(msecs)03d %(levelname)s %(message)s', stream=sys.stderr)
    host = None
    while not host:
        host = broadcastPing(opts)
    logger.info(f"Found logger server at {host}")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.sendto(b"UUL START", (host, opts.port))
    mlen = 0
    while True:
        try:
            data, server = sock.recvfrom(512)
            if data.startswith(b"UUL"):
                logger.debug(f"Got response from {server}: {data}")
            else:
                str = data.decode("utf-8", errors='ignore').strip()
                if (str.startswith("INA:")):
                    if (len(str) > mlen):
                        mlen = len(str)
                    print(str, end='\r')
                else:
                    if (len(str) < mlen):
                        str += ' ' * (mlen - str(len))
                    print(str)
        except socket.timeout:
            logger.debug("Timeout")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verbose", "-v", action="count", default=0)
    parser.add_argument("--host", "-H", default=None)
    parser.add_argument("--port", "-p", default=60606)
    run(*parser.parse_known_args())


if __name__ == "__main__":
    main()