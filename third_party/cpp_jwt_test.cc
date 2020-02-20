#include "src/common/testing/testing.h"
#include <jwt/jwt.hpp>

TEST(CppJWT, TESTHS256) {
  auto key = "secret";
  jwt::jwt_object obj{jwt::params::algorithm("HS256"),
                      jwt::params::payload({{"some", "payload"}}),
                      jwt::params::secret(key)};
  std::string s = obj.signature();
  EXPECT_EQ(s, "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzb21lIjoicGF5bG9hZCJ9.4twFt5NiznN84AWoo1d7KO1T_yoc0Z6XOpOVswacPZg");
}