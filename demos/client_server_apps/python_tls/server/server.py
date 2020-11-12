import socket
import ssl
import time

listen_addr = '127.0.0.1'
listen_port = 8082
server_cert = 'server.crt'
server_key = 'server.key'
# TODO(chengruizhe): Pass the cert implicitly
client_certs = '../python_tls_client/client.crt'

context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
context.verify_mode = ssl.CERT_REQUIRED
context.load_cert_chain(certfile=server_cert, keyfile=server_key)
context.load_verify_locations(cafile=client_certs)

bindsocket = socket.socket()
bindsocket.bind((listen_addr, listen_port))
bindsocket.listen(5)

while True:
    print("Waiting for client")
    newsocket, fromaddr = bindsocket.accept()
    print("Client connected: {}:{}".format(fromaddr[0], fromaddr[1]))
    conn = context.wrap_socket(newsocket, server_side=True)
    print("SSL established.")

    count = 0
    while True:
        time.sleep(1)
        data = conn.recv(1024)
        print(data.decode())
        conn.send("Hello World! {}".format(count).encode())
        count += 1

print("Closing connection")
conn.shutdown(socket.SHUT_RDWR)
conn.close()
