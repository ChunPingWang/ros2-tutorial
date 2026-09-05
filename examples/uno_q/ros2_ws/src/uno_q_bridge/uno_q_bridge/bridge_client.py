"""與 bridge_server.py 溝通的極簡 TCP client（一行一個 JSON）。"""
import json
import socket


class BridgeClient:
    def __init__(self, host: str, port: int, timeout: float = 1.0):
        self._host = host
        self._port = port
        self._timeout = timeout
        self._sock = None
        self._rfile = None

    @property
    def connected(self) -> bool:
        return self._sock is not None

    def connect(self) -> None:
        self.close()
        sock = socket.create_connection((self._host, self._port), timeout=self._timeout)
        self._sock = sock
        self._rfile = sock.makefile('r', encoding='utf-8')

    def close(self) -> None:
        if self._rfile is not None:
            try:
                self._rfile.close()
            finally:
                self._rfile = None
        if self._sock is not None:
            try:
                self._sock.close()
            finally:
                self._sock = None

    def call(self, fn: str, *args):
        if self._sock is None:
            raise ConnectionError('bridge not connected')
        payload = json.dumps({'fn': fn, 'args': list(args)}) + '\n'
        try:
            self._sock.sendall(payload.encode('utf-8'))
            line = self._rfile.readline()
        except OSError as exc:
            self.close()
            raise ConnectionError(str(exc)) from exc
        if not line:
            self.close()
            raise ConnectionError('bridge closed connection')
        resp = json.loads(line)
        if not resp.get('ok', False):
            raise RuntimeError(resp.get('error', 'unknown bridge error'))
        return resp.get('result')
