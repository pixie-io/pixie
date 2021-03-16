package apienv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"

	"pixielabs.ai/pixielabs/src/cloud/scriptmgr/scriptmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services"
)

func init() {
	pflag.String("scriptmgr_service", "scriptmgr-service.plc.svc.local:52000", "The profile service url (load balancer/list is ok)")
}

// NewScriptMgrServiceClient creates a new scriptmgr RPC client stub.
func NewScriptMgrServiceClient() (scriptmgrpb.ScriptMgrServiceClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	authChannel, err := grpc.Dial(viper.GetString("scripts_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return scriptmgrpb.NewScriptMgrServiceClient(authChannel), nil
}
