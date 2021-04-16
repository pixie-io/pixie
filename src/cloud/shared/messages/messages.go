package messages

// This file contains the NATS channels used in Pixie Cloud.

// VizierConnectedChannel is the channel to listen to be notified of Viziers connecting.
// The message passed along this channel is of type px.cloud.messages.VizierConnected.
const VizierConnectedChannel = "VizierConnected"
