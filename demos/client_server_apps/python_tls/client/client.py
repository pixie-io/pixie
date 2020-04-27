import socket
import ssl
import time

host_addr = '127.0.0.1'
host_port = 8082

server_sni_hostname = 'example.com'
client_cert = 'client.crt'
client_key = 'client.key'
# TODO(chengruizhe): Pass the cert implicitly
server_cert = '../python_tls_server/server.crt'

context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH, cafile=server_cert)
context.load_cert_chain(certfile=client_cert, keyfile=client_key)

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
conn = context.wrap_socket(s, server_side=False, server_hostname=server_sni_hostname)
conn.connect((host_addr, host_port))
print("SSL established.")

count = 0
while True:
    time.sleep(1)
    conn.send("Hello! {}".format(count).encode())
    data = conn.recv(1024)
    print(data.decode())
    count += 1

print("Closing connection")
conn.close()
