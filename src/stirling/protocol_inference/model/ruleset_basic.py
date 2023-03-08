# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

"""
This file is a reimplementation of protocol_inference.h
"""
from enum import Enum

import torch
import torch.nn as nn

from model.metadata import kTargetProtocols


class MessageType(Enum):
    kUnknown = 0
    kRequest = 1
    kResponse = 2


def infer_http_message(buf, count):
    if count < 16:
        return MessageType.kUnknown
    if buf[:4] == b"HTTP":
        return MessageType.kResponse
    if buf[:3] == b"GET":
        return MessageType.kRequest
    if buf[:4] == b"POST":
        return MessageType.kRequest
    return MessageType.kUnknown


def infer_cql_message(buf, count):
    kError = 0x00
    kStartup = 0x01
    kReady = 0x02
    kAuthenticate = 0x03
    kOptions = 0x05
    kSupported = 0x06
    kQuery = 0x07
    kResult = 0x08
    kPrepare = 0x09
    kExecute = 0x0a
    kRegister = 0x0b
    kEvent = 0x0c
    kBatch = 0x0d
    kAuthChallenge = 0x0e
    kAuthResponse = 0x0f
    kAuthSuccess = 0x10

    if count < 9:
        return MessageType.kUnknown

    request = (buf[0] & 0x80) == 0x00
    version = buf[0] & 0x7f
    flags = buf[1]
    opcode = buf[4]
    length = int.from_bytes(buf[5:9], byteorder="big")

    if version < 3 or version > 5:
        return MessageType.kUnknown

    if (flags & 0xf0) != 0:
        return MessageType.kUnknown

    if length > 10000:
        return MessageType.kUnknown

    if opcode in [kStartup, kOptions, kQuery, kPrepare, kExecute, kRegister, kBatch, kAuthResponse]:
        if request:
            return MessageType.kRequest
        else:
            return MessageType.kUnknown
    elif opcode in [kError, kReady, kAuthenticate, kSupported, kResult, kEvent, kAuthChallenge,
                    kAuthSuccess]:
        if not request:
            return MessageType.kResponse
        else:
            return MessageType.kUnknown
    return MessageType.kUnknown


def infer_pgsql_startup_message(buf, count):
    kMinMsgLen = 4 + 4 + 4
    if count < kMinMsgLen:
        return MessageType.kUnknown

    kMaxMsgLen = 10240
    length = int.from_bytes(buf[:4], "big")
    if length < kMinMsgLen:
        return MessageType.kUnknown

    if length > kMaxMsgLen:
        return MessageType.kUnknown

    kPgsqlVer30 = b"\x00\x03\x00\x00"
    if buf[4:8] != kPgsqlVer30:
        return MessageType.kUnknown

    kPgsqlUser = b"user"
    if buf[8:12] != kPgsqlUser:
        return MessageType.kUnknown

    return MessageType.kRequest


def infer_pgsql_query_message(buf, count):
    kFrontendTags = [b'Q', b'P', b'B', b'E', b'D', b'C', b'H', b'S', b'F', b'X']
    kBackendTags = [b'R', b'K', b'S', b'1', b'2', b'3', b'C', b'T', b'D', b'I', b'E', b'N', b'Z',
                    b'A', b'V', b'G', b'H']

    if buf[:1] not in kFrontendTags and buf[:1] not in kBackendTags:
        return MessageType.kUnknown

    length = int.from_bytes(buf[1:5], byteorder="big")

    kMinPayloadLen = 8
    kMaxPayloadLen = 30000
    if length < kMinPayloadLen or length > kMaxPayloadLen:
        return MessageType.kUnknown

    if (length + 1 <= count) and (buf[length:] != b'\0'):
        return MessageType.kUnknown
    return MessageType.kRequest


def infer_pgsql_regular_message(buf, count):
    # Assuming sizeof(int32_t) == 4
    kMinMsgLen = 1 + 4
    if count < kMinMsgLen:
        return MessageType.kUnknown
    return infer_pgsql_query_message(buf, count)


def infer_pgsql_message(buf, count):
    type = infer_pgsql_startup_message(buf, count)
    if type != MessageType.kUnknown:
        return type
    return infer_pgsql_regular_message(buf, count)


def infer_mongo_message(buf, count):
    kOPUpdate = 2001
    kOPInsert = 2002
    kReserved = 2003
    kOPQuery = 2004
    kOPGetMore = 2005
    kOPDelete = 2006
    kOPKillCursors = 2007
    kOPCompressed = 2012
    kOPMsg = 2013

    kMongoHeaderLength = 16

    if count < kMongoHeaderLength:
        return MessageType.kUnknown

    messageLength = int.from_bytes(buf[:3], byteorder="little")
    if messageLength < kMongoHeaderLength:
        return MessageType.kUnknown

    requestID = int.from_bytes(buf[4:8], byteorder="little")
    if requestID < 0:
        return MessageType.kUnknown

    responseTo = int.from_bytes(buf[8:12], byteorder="little")
    opcode = int.from_bytes(buf[12:16], byteorder="little")

    if opcode in [kOPUpdate, kOPInsert, kReserved, kOPQuery, kOPGetMore, kOPDelete, kOPKillCursors,
                  kOPCompressed, kOPMsg]:
        if responseTo == 0:
            return MessageType.kRequest

    return MessageType.kUnknown


def infer_mysql_request(buf, count):
    kComQuery = 0x03
    kComconnect = 0x0b
    kComStmtPrepare = 0x16
    kComStmtExecute = 0x17
    kComStmtClose = 0x19

    if count < 5:
        return MessageType.kUnknown

    length = int.from_bytes(buf[:3], byteorder="little")

    seq = buf[3]
    com = buf[4]

    if seq != 0:
        return MessageType.kUnknown

    if length == 0:
        return MessageType.kUnknown

    if length > 10000:
        return MessageType.kUnknown

    if com in [kComconnect, kComQuery, kComStmtPrepare, kComStmtExecute, kComStmtClose]:
        return MessageType.kRequest
    return MessageType.kUnknown


def is_mysql_column_def(buf, count):
    kColumnDefMinLength = 24
    if count < kColumnDefMinLength:
        return False
    return buf[4:8] == b'\x03def'


def infer_mysql_response(buf, count):
    # The reponse for StmtPrepare, StmtExecute, and StmtQuery all contain column def packets
    # if there are columns in the response.
    # dev.mysql.com/doc/internals/en/com-query-response.html#packet-Protocol::ColumnDefinition

    if is_mysql_column_def(buf, count):
        return MessageType.kResponse
    return MessageType.kUnknown


def infer_mysql_message(buf, count):
    type = infer_mysql_request(buf, count)
    if type == MessageType.kUnknown:
        type = infer_mysql_response(buf, count)
    return type


# Reference: https://kafka.apache.org/protocol.html#protocol_messages
# Request Header v0 => request_api_key request_api_version correlation_id
#     request_api_key => INT16
#     request_api_version => INT16
#     correlation_id => INT32
def infer_kafka_request(buf):
    # Note: The Number of Kafka APIs and their versions might change in the future.
    kNumAPIs = 62
    kMaxAPIVersion = 12

    requestAPIKey = int.from_bytes(buf[4:6], byteorder="big")
    if requestAPIKey < 0 or requestAPIKey >= kNumAPIs:
        return MessageType.kUnknown

    requestAPIVersion = int.from_bytes(buf[6:8], byteorder="big")
    if requestAPIVersion < 0 or requestAPIKey > kMaxAPIVersion:
        return MessageType.kUnknown

    correlationID = int.from_bytes(buf[8:12], byteorder="big")
    if correlationID < 0:
        return MessageType.kUnknown

    return MessageType.kRequest


def infer_kafka_message(buf, count):
    # length(4 bytes) + api_key(2 bytes) + api_version(2 bytes) + correlation_id(4 bytes)
    kMinRequestLength = 12
    if count < kMinRequestLength:
        return MessageType.kUnknown

    messageSize = int.from_bytes(buf[:4], byteorder="big")

    if messageSize < 0 or count != (messageSize + 4):
        return MessageType.kUnknown

    return infer_kafka_request(buf)


def infer_dns_message(buf, count):
    kDNSHeaderSize = 12
    kMaxDNSMessageSize = 512
    kMaxNumRR = 25

    if count < kDNSHeaderSize or count > kMaxDNSMessageSize:
        return MessageType.kUnknown

    ubuf = buf
    flags = ubuf[2] << 8 + ubuf[3]
    num_questions = (ubuf[4] << 8) + ubuf[5]
    num_answers = (ubuf[6] << 8) + ubuf[7]
    num_auth = (ubuf[8] << 8) + ubuf[9]
    num_addl = (ubuf[10] << 8) + ubuf[11]

    qr = (flags >> 15) & 0x1
    opcode = (flags >> 11) & 0xf
    zero = (flags >> 6) & 0x1

    if zero != 0:
        return MessageType.kUnknown

    if opcode != 0:
        return MessageType.kUnknown

    if num_questions == 0 or num_questions > 10:
        return MessageType.kUnknown

    num_rr = num_questions + num_answers + num_auth + num_addl
    if num_rr > kMaxNumRR:
        return MessageType.kUnknown

    if qr == 0:
        return MessageType.kRequest
    return MessageType.kResponse


def is_redis_message(buf, count):
    if count < 3:
        return False

    first_byte = buf[:1]

    if first_byte not in [b'+', b'-', b':', b'$', b'*']:
        return False

    if buf[count - 2: count - 1] != b'\r':
        return False

    if buf[count - 1:] != b'\n':
        return False

    return True


def infer_nats_message(buf, count):
    if count < 3:
        return MessageType.kUnknown

    if buf[count - 2] != '\r':
        return MessageType.kUnknown

    if buf[count - 1] != '\n':
        return MessageType.kUnknown

    if buf[:7] == 'CONNECT':
        return MessageType.kRequest

    if buf[:3] == 'SUB':
        return MessageType.kRequest

    if buf[:5] == 'UNSUB':
        return MessageType.kRequest

    if buf[:3] == 'PUB':
        return MessageType.kRequest

    if buf[:4] == 'INFO':
        return MessageType.kResponse

    if buf[:3] == 'MSG':
        return MessageType.kResponse

    if buf[:3] == '+OK':
        return MessageType.kResponse

    if buf[:4] == '-ERR':
        return MessageType.kResponse
    return False


def infer_amqp_message(buf, count):
    kMinFrameLength = 8
    kFrameMethod = 1

    kClassConnection = 10
    kBasicClass = 60

    kMethodConnectionStart = 10
    kMethodConnectionStartOk = 11
    kMethodBasicPublish = 40
    kMethodBasicAck = 80

    if count < kMinFrameLength:
        return MessageType.kUnknown
    frame_type = buf[0]
    if frame_type != kFrameMethod:
        return MessageType.kUnknown

    class_id = int.from_bytes(buf[7:9], "big")
    method_id = int.from_bytes(buf[9:11], "big")

    if class_id == kClassConnection and method_id == kMethodConnectionStart:
        return MessageType.kRequest
    if class_id == kClassConnection and method_id == kMethodConnectionStartOk:
        return MessageType.kResponse
    if class_id == kBasicClass and method_id == kMethodBasicPublish:
        return MessageType.kRequest
    if class_id == kBasicClass and method_id == kMethodBasicAck:
        return MessageType.kResponse
    return MessageType.kUnknown


def infer_mux_message(buf, count):
    replyStatusOk = 0
    replyStatusError = 1
    replyStatusNack = 2
    kTdispatch = 2
    kRdispatch = -2
    kTinit = 68
    kRinit = -68
    kRerr = -128
    kRerrOld = 127
    mux_header_size = 8
    mux_framer_pos = mux_header_size + 6

    if count < mux_header_size:
        return MessageType.kUnknown

    length = int.from_bytes(buf[:4], "big") + 4

    type_and_tag = int.from_bytes(buf[4:8], "big")
    mux_type = (type_and_tag & 0xff000000) >> 24
    tag = (type_and_tag & 0xffffff)

    if mux_type == kTdispatch or mux_type == kTinit or mux_type == kRerrOld:
        msg_type = MessageType.kRequest
    elif mux_type == kRdispatch or mux_type == kRinit or mux_type == kRerr:
        msg_type = MessageType.kResponse
    else:
        return MessageType.kUnknown

    if mux_type == kRerr or mux_type == kRerrOld:
        if buf[length - 5: length] != b'check':
            return MessageType.kUnknown

    if mux_type == kRinit or mux_type == kTinit:
        if buf[mux_framer_pos:mux_framer_pos + 10] != b'mux-framer':
            return MessageType.kUnknown

    if tag < 1 or tag > ((1 << 23) - 1):
        return MessageType.kUnknown

    if mux_type == kTdispatch:
        first_ctx_pos = 12
        if buf[first_ctx_pos:first_ctx_pos + 11] != b'com.twitter':
            return MessageType.kUnknown

    if mux_type == kRdispatch:
        reply_status = int.from_bytes(buf[8], "big")
        if reply_status not in [replyStatusOk, replyStatusError, replyStatusNack]:
            return MessageType.kUnknown

    return msg_type


# TODO(chengruizhe): Instead of maintaining two copies of protocol_inference.h in C and Python,
# directly call C functions with an extension.
def infer_protocol(buf, count):
    if infer_http_message(buf, count) != MessageType.kUnknown:
        return "http"
    elif infer_cql_message(buf, count) != MessageType.kUnknown:
        return "cql"
    elif infer_mongo_message(buf, count) != MessageType.kUnknown:
        return "mongo"
    elif infer_pgsql_message(buf, count) != MessageType.kUnknown:
        return "pgsql"
    elif infer_mysql_message(buf, count) != MessageType.kUnknown:
        return "mysql"
    elif infer_mux_message(buf, count) != MessageType.kUnknown:
        return "mux"
    elif infer_kafka_message(buf, count) != MessageType.kUnknown:
        return "kafka"
    elif infer_dns_message(buf, count) != MessageType.kUnknown:
        return "dns"
    elif is_redis_message(buf, count):
        return "redis"
    elif infer_nats_message(buf, count) != MessageType.kUnknown:
        return "nats"
    elif infer_amqp_message(buf, count) != MessageType.kUnknown:
        return "amqp"
    return "unknown"


protocol2idx = {protocol: i for i, protocol in enumerate(kTargetProtocols)}


class RulesetBasicModel(nn.Module):
    def forward(self, payload_batch):
        result = []
        for payload in payload_batch:
            pred = infer_protocol(payload, len(payload))
            result.append(protocol2idx[pred])
        return torch.LongTensor(result)


class RulesetBasicConnModel(nn.Module):
    def __init__(self):
        super().__init__()

    def forward(self, conn_batch):
        """
        :param conn_batch: 2-dimensional lists of shape batch x num_packets.
        """
        result = []

        for payloads in conn_batch:
            conn_pred = "unknown"
            for payload in payloads:
                pred = infer_protocol(payload, len(payload))
                if pred != "unknown":
                    conn_pred = pred
                    break
            result.append(protocol2idx[conn_pred])
        return torch.LongTensor(result)
