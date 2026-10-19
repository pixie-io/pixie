# PEM direct-query — signing-key security contract

This document is the **authoritative spec for how the JWT signing key flows
through the direct-query endpoint**, what it protects, what it does NOT
protect, the threat model, and the unit-test coverage that locks the contract
down. Pair with [DIRECT_QUERY_CONTRACT.md](DIRECT_QUERY_CONTRACT.md) which
covers the gRPC/PxL surface; this doc covers crypto + key handling only.

## TL;DR

- One HMAC-SHA256 key, source `secret/pl-cluster-secrets/jwt-signing-key`,
  shared by `kelvin`, `query-broker`, `metadata-server`, and (now) the PEM's
  direct-query verifier.
- Mounted into the PEM via `PL_JWT_SIGNING_KEY` env (the existing
  `k8s/vizier/pem/base/pem_daemonset.yaml` `secretKeyRef`).
- Used in both directions: the agent's outgoing service-token mint
  (`manager.cc:GenerateServiceToken`) AND direct-query's incoming
  `verifyHs256Jwt` (`direct_query_server.cc:151`).
- **Compromise of the key = compromise of every in-cluster service-to-service
  auth path.** Direct-query is no more or less protected than kelvin or
  query-broker.

## Key flow

```
                            pl-cluster-secrets/jwt-signing-key
                                          │
       ┌──────────────────────┬───────────┼─────────────┬───────────────────┐
       ▼                      ▼           ▼             ▼                   ▼
   kelvin                query-broker  metadata    vizier-pem            (any future
   (env PL_JWT_…)       (env PL_JWT_…)  (env …)   (env PL_JWT_…)         in-cluster
                                                    │                     consumer)
                                                    │
                              ┌─────────────────────┴─────────────────────┐
                              ▼                                           ▼
                  GenerateServiceToken                    direct-query verifier
                  (outgoing mint to                       (incoming token check
                   kelvin/MDS, manager.cc:434)            from dx_daemon,
                                                          direct_query_server.cc:151)
```

- **Single source of truth:** the `jwt-signing-key` data field in the
  `pl-cluster-secrets` Secret. RBAC restricts read access to the four
  service-account identities above.
- **Two pixie flag pegs reading the same env:**
  - `FLAGS_jwt_signing_key` — DEFINEd in
    `src/vizier/services/agent/shared/manager/manager.cc:60`, owns outgoing
    mint.
  - `FLAGS_direct_query_jwt_signing_key` — DEFINEd in
    `src/vizier/services/agent/pem/pem_manager.cc:47`, owns direct-query
    verification.
- **Fallback:** when `FLAGS_direct_query_jwt_signing_key` is empty,
  `MaybeStartDirectQueryServer` falls back to `FLAGS_jwt_signing_key`. This
  avoids the split-brain where a CLI override of only one flag silently
  disables direct-query auth (CodeRabbit catch on PR #49).
- **Empty-key guard at Init:** `Manager::Init`
  (`shared/manager/manager.cc:140`) refuses to start if
  `FLAGS_jwt_signing_key` is empty. Without this guard, `GenerateServiceToken`
  would throw an uncaught `jwt::SigningError("key not provided")` from
  cpp_jwt's `obj.signature()` on the first outgoing call — observed in the
  field on the stock fork 0.14.17 PEM as a 23-restart CrashLoopBackOff.

## Threat model

### What the signing key protects

| Threat | Mitigation |
|---|---|
| **Unauthenticated direct-query call.** Anyone with network reach to `:50305` opens an ExecuteScript stream. | Bearer JWT required. No token → `UNAUTHENTICATED` at `AuthenticateRequest`. |
| **Wrong-key token (different cluster, stale leak from a rotated secret).** | HMAC verification compares against `effective_signing_key`; mismatch → `UNAUTHENTICATED`. |
| **Expired token replay.** | `exp` claim required; verifier rejects `now ≥ exp_secs`. |
| **alg:none forgery (RFC 8725 §3.1).** Attacker submits unsigned token with `"alg":"none"`. | Verifier requires `alg == "HS256"` in the header; any other value → `UNAUTHENTICATED`. |
| **Wrong-audience token (e.g. one meant for a different service).** | `aud` claim required; verifier rejects anything but `"vizier"` (string) or array-containing `"vizier"`. |
| **Tampered signature / payload / header.** | HMAC-SHA256 verifies the `header.payload` signing input against the supplied signature. Any byte flip in any segment → `UNAUTHENTICATED`. |
| **Token under non-Bearer scheme** (e.g. `Token <jwt>`, `Basic …`). | `stripBearerPrefix` requires case-insensitive `"bearer "`; anything else → token treated as empty → `UNAUTHENTICATED`. |

### What it does NOT protect

| Out of scope | Rationale |
|---|---|
| **Key compromise.** If `pl-cluster-secrets/jwt-signing-key` leaks, an attacker can mint valid tokens for kelvin / query-broker / direct-query at will. | Same threat model as the entire in-cluster service mesh. Out-of-band protections (RBAC on the Secret, sealed-secrets/SOPS at rest, key rotation) are the appropriate controls. |
| **Token replay within the validity window.** A captured valid token is replayable until `exp`. | The `jti` claim is minted by `GenerateServiceToken` but the direct-query verifier does NOT track it (no nonce store). Defensive choice: tokens are short-lived (60s in `GenerateServiceToken`) and in-cluster traffic is TLS-encapsulated, so capture surface is small. If/when we need anti-replay, add a sliding `jti` LRU. |
| **Confidentiality of the PxL query body** + **JWT exposure on the wire**. The JWT only authenticates the caller; the gRPC channel itself carries both the bearer header and the script body. | The :50305 listener uses **cluster-default TLS via `SSL::DefaultGRPCServerCreds()`** (`pem_manager.cc:MaybeStartDirectQueryServer`), reusing the same `tls_ca_crt` + `client_tls_cert` + `client_tls_key` mounts kelvin / metadata / the broker use. Plaintext fallback only when the operator sets `PL_DISABLE_SSL=1` — that is an EXPLICIT dev/soak choice, not a silent default. See "Transport" below. |
| **PEM-level authorization (who can run what PxL).** Any valid token can run any read-only PxL. | Mutations are rejected at the scope guard (`req.mutation() == true → UNIMPLEMENTED`). For read-only queries, the contract is "anyone the cluster trusts to mint a JWT can read PEM data" — same as kelvin's contract. |
| **Cross-tenant isolation in a multi-cluster cloud.** | Out of scope: this is a per-cluster service-to-service token; cross-cluster auth is the cloud's job. |
| **Network-level access control to `:50305`.** | `NetworkPolicy` in the manifest is the right place (out-of-scope for this PR; tracked as a future hardening). |

### Tampering scenarios — unit-test coverage

The cases below are explicit tests in
`src/vizier/services/agent/pem/direct_query_server_test.cc`. Each guards a
specific tampering surface; together they lock the verifier's behaviour
against drift.

| Scenario | Test name | Assertion |
|---|---|---|
| No `authorization` header | `NoToken_Unauthenticated` | `UNAUTHENTICATED` |
| Bearer header with empty token | `BearerEmptyToken_Unauthenticated` | `UNAUTHENTICATED` |
| Lowercase `bearer ` scheme | `ValidToken_LowercaseBearerPrefix_Authenticated` | not `UNAUTHENTICATED` |
| Wrong scheme (`Token <jwt>`) | `WrongAuthScheme_Unauthenticated` | `UNAUTHENTICATED` |
| Garbage non-JWT string | `GarbageBearer_Unauthenticated` | `UNAUTHENTICATED` |
| `alg:none` forgery (RFC 8725) | `AlgNoneToken_Unauthenticated` | `UNAUTHENTICATED` |
| Wrong signing key | `WrongKey_Unauthenticated` | `UNAUTHENTICATED` |
| Expired token | `ExpiredToken_Unauthenticated` | `UNAUTHENTICATED` |
| Missing `aud` claim | `MissingAud_Unauthenticated` | `UNAUTHENTICATED` |
| Wrong `aud` value | `WrongAud_Unauthenticated` | `UNAUTHENTICATED` |
| `aud` as a string (RFC 7519 backwards-compat) | `ValidToken_AudAsString_Authenticated` | not `UNAUTHENTICATED` |
| Missing `exp` claim | `MissingExp_Unauthenticated` | `UNAUTHENTICATED` |
| Single-byte signature flip | `TamperedSignatureByte_Unauthenticated` | `UNAUTHENTICATED` |
| Single-byte payload flip (signature now mismatches) | `TamperedPayloadByte_Unauthenticated` | `UNAUTHENTICATED` |
| Single-byte header flip (signature now mismatches) | `TamperedHeaderByte_Unauthenticated` | `UNAUTHENTICATED` |
| Truncated token (last 10 chars removed) | `TruncatedToken_Unauthenticated` | `UNAUTHENTICATED` |
| Two tokens concatenated (`tok1.tok2`) | `ConcatenatedTokens_Unauthenticated` | `UNAUTHENTICATED` |
| `alg:HS384` confusion (HS256 sig under HS384 header) | `AlgConfusion_HS384_Unauthenticated` | `UNAUTHENTICATED` |
| Mutation flag set | `ValidToken_Mutation_Unimplemented` | `UNIMPLEMENTED` |

The fixture (`DirectQueryServerTest`) hosts an in-process gRPC server backed
by the real `DirectQueryServer`, so the authentication metadata flow is
end-to-end — gRPC client → metadata API → `AuthenticateRequest` → `verifyHs256Jwt`.
There is no mock layer between the test and the verifier.

## Transport — gRPC channel encryption

The direct-query listener is configured with `SSL::DefaultGRPCServerCreds()`
(`src/vizier/services/agent/shared/manager/ssl.cc:67`). That reuses the
PEM's existing cluster TLS pair:

```yaml
# k8s/vizier/pem/base/pem_daemonset.yaml (already in the manifest)
env:
- name: PL_TLS_CA_CERT
  value: /certs/ca.crt
- name: PL_CLIENT_TLS_CERT
  value: /certs/client.crt
- name: PL_CLIENT_TLS_KEY
  value: /certs/client.key
- name: PL_DISABLE_SSL
  value: "false"        # default; flip to "true" only on dev/soak clusters
```

When `PL_DISABLE_SSL` is unset or `false`, `:50305` rejects plaintext
gRPC clients — the JWT bearer never crosses the pod network in the
clear. The same `cert-provisioner` Job that mints kelvin / metadata
TLS pairs already covers the PEM via `pl-cluster-secrets`; no
operator action needed beyond the existing install path.

**Insecure credentials are a deliberate dev-only escape hatch.** If
`PL_DISABLE_SSL=1` is set on a production cluster, the operator has
explicitly opted out — every consumer of the cluster (kelvin / MDS /
direct-query / etc.) drops TLS simultaneously, so the operator
necessarily knows the trade-off. Direct-query does not add a
separate per-feature opt-out.

The runtime soak harness asserts the TLS path:
1. PEM stderr on a healthy install logs `direct-query: step 6/6 grpc
   BuildAndStart on :50305` followed by `direct-query: READY`.
2. `openssl s_client -connect <pem>:50305` returns a valid cluster
   cert chain.
3. A plaintext gRPC call (`grpcurl -plaintext`) is refused with the
   server-side `transport: received unexpected content-type "text/plain"`
   message, confirming TLS-only enforcement.

## Client authentication — how to integrate

The canonical client is `dx_daemon` (`cmd/dx-daemon/pxbroker.go`). The
contract any other client must follow:

1. **Mint a JWT inside the cluster** using the **same `pl-cluster-secrets/
   jwt-signing-key`** the PEM verifies against. The mint helper is
   `src/shared/services/utils/jwt.go`'s `SignJWTClaims` /
   `GenerateJWTForService`. Mount the secret via Kubernetes `secretKeyRef`;
   never hard-code, never read from a ConfigMap, never pass via a CLI
   flag baked into a manifest.
2. **Claim shape** (must match the verifier):
   - `alg=HS256` in the JWT header.
   - `aud` containing the literal string `"vizier"` (array OR plain string
     both work; array is canonical).
   - `exp` numeric, seconds-since-epoch, in the future. Recommended
     lifetime: ≤ 60 seconds.
3. **Send as gRPC metadata** `authorization: Bearer <jwt>`. The Bearer
   scheme is case-insensitive in the verifier — `bearer <jwt>` also works
   — but RFC 6750 Title-case is preferred for interop.
4. **Mint per-call when fan-out > 30 seconds**. Long-lived processes (like
   AE's pixieapi direct-mode `Adapter.Query`) re-mint inside the call,
   not at constructor time, because cpp_jwt locks the key at object
   construction; refreshing means re-instantiating the mint object. The
   60-second `exp` is intentional — even leaked tokens stop working
   within a minute.

### Discouraged practices (and why)

| Practice | Why it's discouraged |
|---|---|
| **Long-lived JWTs** (`exp > 1 hour`, or no `exp`). | A captured token is a permanent credential under TLS-stripped traffic. The 60-second mint window bounds replay damage; longer windows convert direct-query into an always-on backdoor for anyone who sees one token. The verifier rejects no-`exp` outright; long-`exp` is accepted but **strongly** discouraged. |
| **Hard-coding the signing key in code or container images.** | The key flows from `pl-cluster-secrets` → env var → process memory. Anything baked into a layer or git history can be extracted from a compromised registry / repository. Always use `secretKeyRef`. |
| **Reading the signing key from a non-Secret source** (ConfigMap, env var from a manifest literal, S3 bucket, etc.). | Secrets get base64-decoded only at mount-time + receive K8s RBAC restrictions; the alternatives don't. |
| **Logging tokens or the signing key.** | Stderr / k8s logs are not access-controlled the same way the Secret is. The verifier's specific-failure VLOG line is the one place a curious operator can see *which check* failed, and even there only at `--v=1`; the token bytes themselves never appear. |
| **Sharing one token across services.** Mint per-service. | Per-service `ServiceID` (the `sub` claim) is the only audit trail the cluster has for who-called-what. Sharing tokens makes attribution impossible. |
| **Self-signing tokens with a non-cluster key for "testing"**, then leaving the test path in production. | The verifier accepts any token signed with `jwt-signing-key`; if a developer minted with their own key to bypass auth, the production verifier rejects it (good). But if the developer added a code-path that *swaps* the key, the production verifier might accept the leaked test token. Don't refactor the verifier to take a second key. |
| **Calling direct-query from the cloud (kelvin / cloud_connector).** | Direct-query is **node-local**, no broker hop. The cloud path is `kelvin`. Routing cloud → direct-query bypasses kelvin's cloud-side authorization and skips per-cluster API-key checks. The two paths have different threat models — keep them separate. |
| **Bypassing the Bearer scheme** (e.g. sending the JWT as a raw header value). | The verifier requires `bearer ` (case-insensitive) before the token. Raw values are rejected; future versions may accept different schemes (mTLS), and the scheme delimiter is what gives us forward-compat. |

## Disabling the feature

Two levels of disable. Pick the right one for your threat model:

### Runtime disable (soft, per-deploy)

```yaml
# In the Vizier CR, or as a direct env on the PEM DaemonSet:
env:
- name: PL_PEM_DIRECT_QUERY_ENABLED
  value: "false"        # default
```

When `--direct_query_enabled=false`:
- `:50305` is never bound (`builder.AddListeningPort` is never called).
- The dedicated direct-query Carnot is never constructed.
- The JWT verifier is loaded into the binary but never reached by a
  request — the gRPC service exists but no request can route to it
  because the service is never `RegisterService`'d.
- The runtime flag is the per-deploy toggle. Cluster operators who
  want direct-query in some clusters but not others use this.

### Compile-time disable (hard, per-binary)

```bash
bazel build //src/vizier/services/agent/pem:pem_image \
    --//src/vizier/services/agent/pem:direct_query=false
```

The `:direct_query` `bool_flag` defaults to `True`, so ordinary builds
compile the feature in and rely on the runtime flag above. Setting it to
`false` matches the `:direct_query_disabled` `config_setting` in
`src/vizier/services/agent/pem/BUILD.bazel`, which puts
`PX_PEM_DIRECT_QUERY_DISABLED` on this package's compilations (and on the
endpoint's test target). When that macro is defined:

- The **entire feature-bearing body** of `direct_query_server.cc` is
  excluded via `#ifndef`: no openssl HMAC, no rapidjson, no JWT
  verifier, no Carnot driver, no drain loop. The binary carries only
  ~50 bytes of stub `AuthenticateRequest` / `ExecuteScript` that return
  `UNAUTHENTICATED` / `UNIMPLEMENTED` so the class still resolves at
  link time.
- `pem_manager.cc`'s flag DEFINEs are excluded — `--direct_query_enabled`,
  `--direct_query_port`, `--direct_query_jwt_signing_key` do not exist
  in this build's gflags registry. Trying to pass them on the CLI
  yields "unknown flag" at startup.
- `MaybeStartDirectQueryServer` returns `Status::OK()` after a single
  LOG line; no carnot construction, no port binding, no thread starts.
- **Use this when shipping to customers / sectors who must not have
  the feature available even as a disable-by-default option.** The
  feature is gone from the binary; no runtime configuration can
  re-enable it.

### Effectiveness asserted by unit tests

- `direct_query_server_test.cc::CompiledOut_ValidToken_StillUnauthenticated`
  proves that even a freshly-minted, signed-by-the-cluster JWT cannot
  re-enable the feature in a disabled build — the auth path short-
  circuits before reaching the JWT verifier.
- `direct_query_server_test.cc::CompiledOut_NoToken_Unauthenticated`
  proves the same for the trivial no-token case.
- `direct_query_server_test.cc::ToggleContract_DocumentBothLevels` is
  the default-build documentary book-end naming both toggle levels.

### Cleanup of in-flight references when disabling

- If you flip the **runtime** flag from `true` → `false` on a live
  cluster, the existing direct-query gRPC server keeps running until
  the PEM pod restarts. The flag is read at PEMManager init.
  Cleanup = roll the DaemonSet.
- If you flip the **compile-time** macro, the redeployed image has no
  direct-query at all; old binaries on rolling-update nodes continue
  to serve direct-query until they cycle out. No partial state — each
  binary is wholly enabled or wholly disabled.

## Failure modes — what each auth failure looks like to a client

| Client-observed gRPC status | Server-side cause | Operator action |
|---|---|---|
| `UNAUTHENTICATED` "direct-query: invalid bearer token" | Any verifier failure (sig mismatch, expired, wrong aud, etc.). Specific reason is `VLOG(1)`'d in PEM stderr. | Check `--v=1` PEM stderr for the specific check; usually a clock skew, wrong key (rotated secret), or token shape mismatch. |
| `UNAUTHENTICATED` "missing authorization metadata" | Caller didn't send the `authorization` header. | Client bug. Add `Bearer <jwt>` to gRPC metadata. |
| `UNAUTHENTICATED` "authorization is not a Bearer token" | Header present but not `Bearer ...`. | Client bug. Use the Bearer scheme. |
| `UNIMPLEMENTED` "mutations are out of scope (read-only endpoint)" | `req.mutation == true`. | Direct-query is read-only by design. Use the broker path for mutations. |
| `UNIMPLEMENTED` "compiled out of this build (PX_PEM_DIRECT_QUERY_DISABLED)" | This PEM was built with the compile-time disable macro. | Either rebuild with the feature enabled, or use the broker path. |
| `FAILED_PRECONDITION` "server not wired with a live Carnot" | `MaybeStartDirectQueryServer` failed during init (one of `step 1/6 … 6/6` breadcrumbs in PEM stderr will be the last line printed). Direct-query stayed fail-soft; the gRPC service exists but Carnot was never wired. | Check PEM stderr's breadcrumbs to see which step failed; fix the underlying cause (port collision, JWT key empty, Carnot::Create error). |
| `INVALID_ARGUMENT` "PxL compile failed (...)" | The PxL is syntactically invalid or refers to an unknown table. | Fix the PxL. |
| `INTERNAL` "PxL execute failed (...)" | Carnot's exec path failed mid-stream. | Check the error message; often a table_store inconsistency or a UDF panic. |

## Key rotation

Rotation = update the Secret + restart the consumers (kelvin, query-broker,
metadata-server, vizier-pem). dx_daemon picks up the new key on its next
mint. There is **no overlap window** — both old and new tokens would have to
verify simultaneously, which the current verifier doesn't support (no
multi-key). For a zero-downtime rotation, the contract would need to:

1. Hold two valid keys at once (`PL_JWT_SIGNING_KEY_PREV` + `_NEW`).
2. Mint with `_NEW`; verify against either.
3. After `max(exp_window)`, drop `_PREV`.

Not implemented today. Track as a hardening follow-up if/when key rotation
becomes operational.

## Logging / observability

- The verifier's specific failure reason (e.g. "signature mismatch", "wrong
  audience") is `VLOG(1)`'d in `direct_query_server.cc:243` but **collapsed
  on the wire** to a generic `"direct-query: invalid bearer token"`. Peers
  cannot probe which check failed; operators can flip
  `--v=1` to see the diagnostic in PEM stderr.
- `MaybeStartDirectQueryServer`'s breadcrumbs (`step 1/6 … 6/6 READY`) name
  the exact step on init failure — but they do NOT log the key value. The
  empty-key guard logs `"signing key is empty"`, not the key.
- **Never** include the signing key in PEM stderr, logs, or status messages.
  No `LOG(*) << FLAGS_jwt_signing_key` anywhere; reviewers should reject any
  patch that adds one.

## Cross-references

- `src/vizier/services/agent/pem/direct_query_server.cc:151` —
  `verifyHs256Jwt` implementation.
- `src/vizier/services/agent/pem/pem_manager.cc:47, :132` — flag DEFINE +
  effective-key fallback.
- `src/vizier/services/agent/shared/manager/manager.cc:60, :140, :440` —
  shared flag DEFINE + empty-key guard + outgoing-mint
  `GenerateServiceToken`.
- `k8s/vizier/pem/base/pem_daemonset.yaml:98-102` — `PL_JWT_SIGNING_KEY`
  `secretKeyRef` mount.
- `k8s/vizier/base/{kelvin,query_broker}_deployment.yaml` — sibling consumers
  of the same secret key.
- `src/vizier/services/agent/pem/DIRECT_QUERY_CONTRACT.md` — sibling doc on
  the gRPC/PxL contract.
