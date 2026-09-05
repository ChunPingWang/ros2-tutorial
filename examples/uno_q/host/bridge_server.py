#!/usr/bin/env python3
"""
UNO Q Linux 側的橋接服務。

在 App Lab 的 Python 環境中執行，把 Bridge RPC 暴露成本機 TCP 介面，
讓 Docker 容器內的 ROS 2 節點可以呼叫 MCU 側的函式。

協定：一行一個 JSON。
  請求：{"fn": "set_led", "args": [true]}
  回應：{"ok": true, "result": true}
        {"ok": false, "error": "..."}
"""
import json
import socketserver
import threading

try:
    from arduino.app_utils import Bridge  # App Lab 提供
except ImportError:  # 方便在沒有 App Lab 的機器上測試協定
    class _FakeBridge:
        @staticmethod
        def call(fn, *args):
            print(f"[fake] Bridge.call({fn}, {args})")
            return 0 if fn == "read_analog" else args[0] if args else None
    Bridge = _FakeBridge()

HOST = "127.0.0.1"
PORT = 5555

# Bridge 通道一次只服務一個呼叫，用鎖避免多個 client 交錯。
_bridge_lock = threading.Lock()


class BridgeHandler(socketserver.StreamRequestHandler):
    def handle(self):
        peer = self.client_address
        print(f"client connected: {peer}")
        for raw in self.rfile:
            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue
            try:
                req = json.loads(line)
                fn = req["fn"]
                args = req.get("args", [])
                with _bridge_lock:
                    result = Bridge.call(fn, *args)
                resp = {"ok": True, "result": result}
            except Exception as exc:  # noqa: BLE001
                resp = {"ok": False, "error": str(exc)}
            self.wfile.write((json.dumps(resp) + "\n").encode("utf-8"))
            self.wfile.flush()
        print(f"client disconnected: {peer}")


class ThreadedServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    with ThreadedServer((HOST, PORT), BridgeHandler) as server:
        print(f"bridge server listening on {HOST}:{PORT}")
        server.serve_forever()


if __name__ == "__main__":
    main()
