package main

import (
	"fmt"
	"time"

	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func main() {
	services.PostFlagSetupAndParse()
	token := testingutils.GenerateTestJWTTokenWithDuration(nil, viper.GetString("jwt_signing_key"), time.Hour*24*30)
	fmt.Printf("%s\n", token)
}
