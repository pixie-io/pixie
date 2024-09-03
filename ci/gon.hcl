// This file defines the config for getting the CLI signed
// using Gon.

source = ["./cli_darwin_amd64", "./cli_darwin_arm64", "cli_darwin_universal"]
bundle_id = "ai.getcosmic.px"

apple_id {
  username = "apple-dev@getcosmic.ai"
  password = "@env:AC_PASSWD"
  provider = "769M9XJDG6"
}

sign {
  application_identity = "Developer ID Application: Cosmic Observe, Inc."
}

zip {
  output_path = "cli_darwin.zip"
}
