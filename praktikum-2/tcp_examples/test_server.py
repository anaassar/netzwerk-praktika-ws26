import socket

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

server.bind(("127.0.0.1", 12345))
server.listen(1)

print("Waiting for client...")

connection, address = server.accept()

print("Client connected:", address)

while True:
    data = connection.recv(4096)

    if not data:
        print("Client finished sending.")

        connection.sendall(
            b"Hello! This message was sent after your shutdown.\n"
        )

        break

    print("Received:", data.decode(errors="replace"))

connection.close()
server.close()