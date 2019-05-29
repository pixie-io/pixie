// Copyright 2017-2018 The NATS Authors
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This is copied from
// https://github.com/nats-io/cnats/blob/3b4668698b8510b8f08413a94523b05a8036d9ab/examples/getstarted/replier.c
// for the sake of testing that cnats builds correctly.

#include <nats/nats.h>

static void onMsg(natsConnection* nc, natsSubscription* /*sub*/, natsMsg* msg, void* closure) {
  printf("Received msg: %s - %.*s\n", natsMsg_GetSubject(msg), natsMsg_GetDataLength(msg),
         natsMsg_GetData(msg));

  // Sends a reply
  if (natsMsg_GetReply(msg) != NULL) {
    natsConnection_PublishString(nc, natsMsg_GetReply(msg), "here's some help");
  }

  // Need to destroy the message!
  natsMsg_Destroy(msg);

  // Notify the main thread that we are done.
  *reinterpret_cast<bool*>(closure) = true;
}

int main(int /*argc*/, char** /*argv*/) {
  natsConnection* conn = NULL;
  natsSubscription* sub = NULL;
  natsStatus s;
  volatile bool done = false;

  printf("Listening for requests on subject 'help'\n");

  // Creates a connection to the default NATS URL
  s = natsConnection_ConnectTo(&conn, NATS_DEFAULT_URL);
  if (s == NATS_OK) {
    // Creates an asynchronous subscription on subject "help",
    // waiting for a request. If a message arrives on this
    // subject, the callback onMsg() will be invoked and it
    // will send a reply.
    void* d = const_cast<void*>(static_cast<volatile void*>(&done));
    s = natsConnection_Subscribe(&sub, conn, "help", onMsg, d);
  }
  if (s == NATS_OK) {
    for (; !done;) {
      nats_Sleep(100);
    }
  }

  // Anything that is created need to be destroyed
  natsSubscription_Destroy(sub);
  natsConnection_Destroy(conn);

  // If there was an error, print a stack trace and exit
  if (s != NATS_OK) {
    nats_PrintLastErrorStack(stderr);
    exit(2);
  }

  return 0;
}
