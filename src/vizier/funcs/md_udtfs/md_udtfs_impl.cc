#include "src/vizier/funcs/md_udtfs/md_udtfs_impl.h"

#include <memory>
#include <string>
#include <utility>

#include <jwt/jwt.hpp>
#include <sole.hpp>
#include "src/common/base/base.h"

// TODO(zasgar): This is kinda hacky. Need to refactor when we figure out auth story.
DEFINE_string(jwt_signing_key, gflags::StringFromEnv("PL_JWT_SIGNING_KEY", ""),
              "The JWT signing key for outgoing requests");

namespace pl {
namespace vizier {
namespace funcs {
namespace md {

std::string GenerateServiceToken() {
  jwt::jwt_object obj{jwt::params::algorithm("HS256")};
  obj.add_claim("iss", "PL");
  obj.add_claim("aud", "service");
  obj.add_claim("jti", sole::uuid4().str());
  obj.add_claim("iat", std::chrono::system_clock::now());
  obj.add_claim("nbf", std::chrono::system_clock::now() - std::chrono::seconds{60});
  obj.add_claim("exp", std::chrono::system_clock::now() + std::chrono::seconds{60});
  obj.add_claim("sub", "service");
  obj.add_claim("Scopes", "service");
  obj.add_claim("ServiceID", "kelvin");
  obj.secret(FLAGS_jwt_signing_key);
  return obj.signature();
}

}  // namespace md
}  // namespace funcs
}  // namespace vizier
}  // namespace pl
