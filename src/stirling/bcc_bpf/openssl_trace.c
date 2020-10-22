// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

/***********************************************************
 * General helpers
 ***********************************************************/

static __inline void process_openssl_data(struct pt_regs* ctx, uint64_t id,
                                          const enum TrafficDirection direction,
                                          const struct data_args_t* args) {
  ssize_t bytes_count = PT_REGS_RC(ctx);
  process_data(/* vecs */ false, ctx, id, direction, args, bytes_count, /* ssl */ true);
}

static __inline void set_conn_as_ssl(uint64_t id, uint32_t fd) {
  uint32_t tgid = id >> 32;
  // Update conn_info, so that encrypted data data can be filtered out.
  struct conn_info_t* conn_info = get_conn_info(tgid, fd);
  if (conn_info == NULL) {
    return;
  }
  conn_info->ssl = true;
}

/***********************************************************
 * Argument parsing helpers
 ***********************************************************/

static int get_fd(void* ssl) {
  // Extract FD via ssl->rbio->num.

  // First, we need to know the offsets of the members within their structs.

  // Some useful links, for different OpenSSL versions:
  // 1.1.0a:
  // https://github.com/openssl/openssl/blob/ac2c44c6289f9716de4c4beeb284a818eacde517/<filename>
  // 1.1.0l:
  // https://github.com/openssl/openssl/blob/7ea5bd2b52d0e81eaef3d109b3b12545306f201c/<filename>
  // 1.1.1a:
  // https://github.com/openssl/openssl/blob/d1c28d791a7391a8dc101713cd8646df96491d03/<filename>
  // 1.1.1e:
  // https://github.com/openssl/openssl/blob/a61eba4814fb748ad67e90e81c005ffb09b67d3d/<filename>

  // Offset of rbio in struct ssl_st.
  // Struct is defined in ssl/ssl_local.h, ssl/ssl_locl.h, ssl/ssl_lcl.h, depending on the version.
  // Verified to be valid for following versions:
  //  - 1.1.0a to 1.1.0k
  //  - 1.1.1a to 1.1.1e
  const int kSSLRBIOOffset = 0x10;

  // Offset of num in struct bio_st.
  // Struct is defined in bio/bio_lcl.h, bio/bio_local.h depending on the version.
  // Only verified to be valid for following versions:
  //  - 1.1.1a to 1.1.1e
  // In version 1.1.0, the offset may be at 0x2c (by inspection, unverified).
  const int kRBIOFDOffset = 0x30;

  const void** rbio_ptr_addr = (ssl + kSSLRBIOOffset);
  const void* rbio_ptr = *rbio_ptr_addr;
  const int* rbio_num_addr = rbio_ptr + kRBIOFDOffset;
  const int rbio_num = *rbio_num_addr;

  return rbio_num;
}

// Appendix: Using GDB to confirm member offsets:
// (gdb) p s
// $18 = (SSL *) 0x55ea646953c0
// (gdb) p &s.rbio
// $22 = (BIO **) 0x55ea646953d0
// (gdb) p s.rbio
// $23 = (BIO *) 0x55ea64698b10
// (gdb) p &s.rbio.num
// $24 = (int *) 0x55ea64698b40
// (gdb) p s.rbio.num
// $25 = 3
// (gdb) p &s.wbio
// $26 = (BIO **) 0x55ea646953d8
// (gdb) p s.wbio
// $27 = (BIO *) 0x55ea64698b10
// (gdb) p &s.wbio.num
// $28 = (int *) 0x55ea64698b40
// (gdb) p s.wbio.num

/***********************************************************
 * BPF probe function entry-points
 ***********************************************************/

BPF_HASH(active_ssl_read_args_map, uint64_t, struct data_args_t);
BPF_HASH(active_ssl_write_args_map, uint64_t, struct data_args_t);

// Function signature being probed:
// int SSL_write(SSL *ssl, const void *buf, int num);
int probe_entry_SSL_write(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  void* ssl = (void*)PT_REGS_PARM1(ctx);
  char* buf = (char*)PT_REGS_PARM2(ctx);

  struct data_args_t write_args = {};
  write_args.fd = get_fd(ssl);
  write_args.buf = buf;
  active_ssl_write_args_map.update(&id, &write_args);

  // Mark connection as SSL right away, so encrypted traffic does not get traced.
  set_conn_as_ssl(id >> 32, write_args.fd);

  return 0;
}

int probe_ret_SSL_write(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  const struct data_args_t* write_args = active_ssl_write_args_map.lookup(&id);
  if (write_args != NULL) {
    process_openssl_data(ctx, id, kEgress, write_args);
  }

  active_ssl_write_args_map.delete(&id);
  return 0;
}

// Function signature being probed:
// int SSL_read(SSL *s, void *buf, int num)
int probe_entry_SSL_read(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  void* ssl = (void*)PT_REGS_PARM1(ctx);
  char* buf = (char*)PT_REGS_PARM2(ctx);

  struct data_args_t read_args = {};
  read_args.fd = get_fd(ssl);
  read_args.buf = buf;
  active_ssl_read_args_map.update(&id, &read_args);

  // Mark connection as SSL right away, so encrypted traffic does not get traced.
  set_conn_as_ssl(id >> 32, read_args.fd);

  return 0;
}

int probe_ret_SSL_read(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  const struct data_args_t* read_args = active_ssl_read_args_map.lookup(&id);
  if (read_args != NULL) {
    process_openssl_data(ctx, id, kIngress, read_args);
  }

  active_ssl_read_args_map.delete(&id);
  return 0;
}
