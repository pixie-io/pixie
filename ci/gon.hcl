// This file defines the config for getting the CLI signed
// using Gon.

source = ["./cli_darwin_amd64", "./cli_darwin_arm64", "cli_darwin_universal"]
bundle_id = "ai.pixielabs.px"

// TODO(zasgar): Update this to the orders@pixielabs.ai account. It has access to the certs,
// but does not have access to application passwords.
apple_id {
  username = "zasgar@gmail.com"
  password = "@env:AC_PASSWD"
}

sign {
  application_identity = "Developer ID Application: Pixie Labs Inc. (SZCNTABEXY)"
}

zip {
  output_path = "cli_darwin.zip"
}
