package main

import (
	"fmt"
	"time"

	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/services/common"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func main() {
	common.PostFlagSetupAndParse()
	token := testingutils.GenerateTestJWTTokenWithDuration(nil, viper.GetString("jwt_signing_key"), time.Hour*24*30)
	fmt.Printf("Token is: %s\n", token)
}
