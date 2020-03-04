package messagebus

import (
	"fmt"
)

// C2VTopicPrefix is the prefix for all message topics from cloud domain to local NATS domain.
const C2VTopicPrefix = "c2v"

// V2CTopicPrefix is the prefix for all message topics sent from local NATS to cloud domain.
const V2CTopicPrefix = "v2c"

// V2CTopic returns the topic used in the Vizier NATS domain to send messages from Vizier to Cloud.
func V2CTopic(topic string) string {
	// TODO(zasgar/michelle): Add validation.
	return fmt.Sprintf("%s.%s", V2CTopicPrefix, topic)
}

// C2VTopic returns the topic used in the Vizier NATS domain to get messages from the Cloud.
func C2VTopic(topic string) string {
	// TODO(zasgar/michelle): Add validation.
	return fmt.Sprintf("%s.%s", C2VTopicPrefix, topic)
}
