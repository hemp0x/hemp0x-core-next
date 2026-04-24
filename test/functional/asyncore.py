#!/usr/bin/env python3
# Copyright (c) 2026 The Hemp0x developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Small asyncore compatibility layer for the legacy functional tests.

Python removed the stdlib asyncore module after 3.11. The Hemp0x functional
P2P harness only needs a narrow subset of asyncore.dispatcher and asyncore.loop,
so keep that subset local instead of pinning the test suite to old Python.
"""

import select
import socket

socket_map = {}


class dispatcher:
    def __init__(self, sock=None, map=None):
        self._map = socket_map if map is None else map
        self.socket = None
        if sock is not None:
            self.set_socket(sock)

    def set_socket(self, sock):
        self.socket = sock
        self.socket.setblocking(False)
        self.add_channel()

    def create_socket(self, family=socket.AF_INET, type=socket.SOCK_STREAM):
        self.set_socket(socket.socket(family, type))

    def add_channel(self):
        self._map[self.socket.fileno()] = self

    def del_channel(self):
        if self.socket is not None:
            self._map.pop(self.socket.fileno(), None)

    def connect(self, address):
        err = self.socket.connect_ex(address)
        if err not in (0, 10035, 10036, 10037, 115,  EINPROGRESS):
            raise OSError(err, "connect_ex failed")

    def close(self):
        self.del_channel()
        if self.socket is not None:
            self.socket.close()
            self.socket = None

    def recv(self, buffer_size):
        return self.socket.recv(buffer_size)

    def send(self, data):
        return self.socket.send(data)

    def readable(self):
        return True

    def writable(self):
        return False

    def handle_read(self):
        pass

    def handle_write(self):
        pass

    def handle_close(self):
        self.close()

    def handle_error(self):
        self.handle_close()


EINPROGRESS = getattr(__import__("errno"), "EINPROGRESS", 115)


def loop(timeout=30.0, use_poll=False, map=None, count=None):
    del use_poll
    channels = socket_map if map is None else map
    iterations = 0

    while channels and (count is None or iterations < count):
        iterations += 1
        readers = []
        writers = []
        errors = []

        for obj in list(channels.values()):
            if obj.socket is None:
                continue
            if obj.readable():
                readers.append(obj.socket)
            if obj.writable():
                writers.append(obj.socket)
            errors.append(obj.socket)

        if not readers and not writers and not errors:
            return

        try:
            readable, writable, exceptional = select.select(readers, writers, errors, timeout)
        except OSError:
            for obj in list(channels.values()):
                obj.handle_error()
            return

        by_fd = {obj.socket.fileno(): obj for obj in list(channels.values()) if obj.socket is not None}

        for sock in exceptional:
            obj = by_fd.get(sock.fileno())
            if obj is not None:
                obj.handle_close()

        for sock in readable:
            obj = by_fd.get(sock.fileno())
            if obj is not None:
                try:
                    obj.handle_read()
                except OSError:
                    obj.handle_close()

        for sock in writable:
            obj = by_fd.get(sock.fileno())
            if obj is not None:
                try:
                    obj.handle_write()
                except OSError:
                    obj.handle_close()
