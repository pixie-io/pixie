// Reference: https://wiki.openssl.org/index.php/SSL/TLS_Client
#include <arpa/inet.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "src/common/base/base.h"

int create_socket(int port) {
  int s;
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  const char localhost[] = "127.0.0.1";
  if (inet_pton(AF_INET, localhost, &addr.sin_addr) <= 0) {
    perror("An inet_pton error occured");
    exit(EXIT_FAILURE);
  }

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("Unable to create socket");
    exit(EXIT_FAILURE);
  }

  if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("Unable to connect");
    exit(EXIT_FAILURE);
  }

  return s;
}

void init_openssl() {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() { EVP_cleanup(); }

SSL_CTX* create_context() {
  const SSL_METHOD* method;
  SSL_CTX* ctx;

  method = SSLv23_client_method();

  ctx = SSL_CTX_new(method);
  if (!ctx) {
    perror("Unable to create SSL context");
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }

  return ctx;
}

int main() {
  int sock;
  SSL_CTX* ctx;

  init_openssl();
  ctx = create_context();
  SSL_CTX_set_ecdh_auto(ctx, 1);
  sock = create_socket(4433);

  // Establish TLS connection
  SSL* ssl;
  ssl = SSL_new(ctx);
  SSL_set_fd(ssl, sock);

  if (SSL_connect(ssl) <= 0) {
    ERR_print_errors_fp(stderr);
  } else {
    std::cout << "TLS connection established." << std::endl;
  }

  char buf[64] = {};
  const std::string_view r1 = "Request ";
  std::string request;
  int count = 0;
  while (1) {
    usleep(1000000);
    request = absl::StrCat(r1, count);
    SSL_write(ssl, request.c_str(), request.size());
    SSL_read(ssl, buf, 32);
    std::cout << buf << std::endl;
    ++count;
  }

  SSL_shutdown(ssl);
  SSL_free(ssl);
  close(sock);
  SSL_CTX_free(ctx);
  cleanup_openssl();
}
