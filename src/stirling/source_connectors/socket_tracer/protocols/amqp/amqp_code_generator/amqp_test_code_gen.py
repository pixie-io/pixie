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

import os
import subprocess
import json
from pathlib import Path
from typing import OrderedDict
from collections import namedtuple

import fire
import pyshark
import dill

from px.src.stirling.source_connectors.socket_tracer.protocols.amqp.amqp_code_generator.amqp_code_gen import (
    AMQPMethod,
    CodeGeneratorWriter,
    CodeGenerator,
    FieldType,
)


class FrameTypes:
    method = 1
    content_header = 2
    content_body = 3
    hearbeat = 8


MAX_NUM_PACKETS_PARSED = 100


class UniquePacketData:
    """
    Records a single packet for each type of frame/method. This prevents running duplicate tests

    heartbeat_packet - packet with heartbeat
    content_body_packets - packet with { AMQP Class : packet }
    content_header_packets - packet with { AMQP Class : packet }
    method_packets - packet with { AMQP Class : { method : packet } }
    """

    def __init__(self) -> None:
        self.heartbeat_packet = None
        self.content_body_packets = {}
        self.content_header_packets = {}
        self.method_packets = {}


AMQPMethodKey = namedtuple(
    "AMQPMethodKey", ["class_id", "method_id"]
)  # Tuple used to store generated AMQP class id/method id


class CodeTestGenerator:
    """
    Uses the AMQP generation and packet capture to generate tests
    """

    def __init__(
        self,
        xml_file="amqp0-9-1.xml",
        pcap_file="amqp_sample_capture.pcapng",
        generation_dir="generated_files",
        gen_template_dir="gen_templates",
    ):
        print("Parsing Generator..")
        self.generator_writer: CodeGeneratorWriter = CodeGeneratorWriter(
            xml_file, generation_dir, gen_template_dir
        )
        self.generator: CodeGenerator = self.generator_writer.generator
        self.generation_dir = self.generator_writer.generation_dir
        self.template_dir = self.generator_writer.template_dir
        self.env = self.generator_writer.env

        full_bzl_path = os.path.dirname(self.template_dir)
        self.pcap_file = os.path.join(full_bzl_path, pcap_file)
        self.parse_id_to_method_mapping()
        self.parse_decode_test_gen_path = Path(self.generation_dir) / Path(
            "parse_decode_test.cc"
        )
        self.unique_packet_data_path = Path(self.generation_dir) / Path(
            "unique_packet_data.dill"
        )

    def parse_id_to_method_mapping(self):
        """
        Converts the raw xml format with id mappings to a dictionary of
            { class id : class object }
            and
            { (class id, method_id) : method object }
        """
        self.amqp_clas_id_class = {}
        self.amqp_class_id_method_id_method = {}
        for amqp_class in self.generator.amqp_classes:
            self.amqp_clas_id_class[amqp_class.class_id] = amqp_class
            for method in amqp_class.methods:
                amqp_method_key = AMQPMethodKey(
                    class_id=str(amqp_class.class_id), method_id=str(method.method_id)
                )
                self.amqp_class_id_method_id_method[amqp_method_key] = method

    def str_packet_to_hex(self, packet):
        """
        Converts packet from wireshark to hex
        """
        if isinstance(packet, list):
            # Some cases the parsing fails and it returns the form ['abce", 88, 73, ..]
            packet = packet[0]
        packet_chunks = [
            "\\x{:02x}".format(int(p1, 16) * 16 + int(p2, 16))
            for p1, p2 in zip(packet[::2], packet[1::2])
        ]
        return "".join(packet_chunks)

    def read_packet_capture(self):
        """
        Uses pyshark api to convert the pcap file to list of packets.
        The raw version hold the bytes and unraw holds the list of fields
        """
        self.cap_raw = pyshark.FileCapture(
            self.pcap_file,
            display_filter="amqp",
            include_raw=True,
            use_json=True,
        )
        self.cap = pyshark.FileCapture(
            self.pcap_file,
            display_filter="amqp",
        )

    def parse_packets(self):
        """
        Parses the pcap file and saves the result as a pickled object.
        A pickle is used since the operation is slow/expensive.
        """
        print("Parsing pcap file to raw capture...")
        self.read_packet_capture()

        unique_packet_data = UniquePacketData()
        for cap_num in range(MAX_NUM_PACKETS_PARSED):
            packet_raw = self.cap_raw[cap_num]["AMQP_RAW"]
            packet = self.cap[cap_num]["AMQP"]
            frame_type = int(packet.get_field_value("type"))

            packet_bytes = self.str_packet_to_hex(packet_raw.get("value"))
            cap_value = (packet, packet_bytes)

            if (
                frame_type == FrameTypes.hearbeat
                and not unique_packet_data.heartbeat_packet
            ):
                unique_packet_data.heartbeat_packet = cap_value
            elif frame_type == FrameTypes.method:
                # packet for methods have method_class(class_id) and method_method(method_id)
                method_class = int(packet.get_field_value("method_class"))
                method_method = int(packet.get_field_value("method_method"))
                amqp_method_key = AMQPMethodKey(
                    class_id=str(method_class), method_id=str(method_method)
                )
                if amqp_method_key not in unique_packet_data.method_packets:
                    unique_packet_data.method_packets[amqp_method_key] = cap_value
            elif frame_type == FrameTypes.content_header:
                # packet for content header have header_class
                header_class = int(packet.get_field_value("header_class"))
                if header_class not in unique_packet_data.content_header_packets:
                    unique_packet_data.content_header_packets[header_class] = cap_value
            elif (
                frame_type == FrameTypes.content_body
                and not unique_packet_data.content_body_packets
            ):
                unique_packet_data.content_body_packets = cap_value

        with open(self.unique_packet_data_path, "wb") as f:
            dill.dump(unique_packet_data, f)

    def gen_test(self, test_name, raw_packet_value, msg):
        """
        creates a tuples for the google test.
        """
        return f"""
            std::make_tuple(
                "{test_name}",
                CreateStringView<char>("{raw_packet_value}"),
                "{msg}"
            ),
        """

    def gen_content_body_test(self, content_body_packets):
        """
        Generates a tuple for content body packet
        """
        content_body_details, content_body_value = content_body_packets
        frame_type = content_body_details.get_field_value("type")
        channel = content_body_details.get_field_value("channel")
        payload_size = content_body_details.get_field_value("Length")

        return self.gen_test(
            test_name="content_body",
            raw_packet_value=content_body_value,
            msg=f"frame_type=[{frame_type}] channel=[{channel}] payload_size=[{payload_size}] msg=[]",
        )

    def gen_heartbeat_test(self, heartbeat_packet):
        """
        Heartbeat format is set and always has frame type 8, channel 0, payload size 0, and no payload.
        """
        _, heartbeat_value = heartbeat_packet
        return self.gen_test(
            test_name="hearbeat_test",
            raw_packet_value=heartbeat_value,
            msg="frame_type=[8] channel=[0] payload_size=[0] msg=[]",
        )

    def get_default_value_field(self, field_type):
        """
        Default value as part of frame_type=[""] channel=[0] payload_size=[0] msg=[]
        """
        if (
            field_type == FieldType.shortstr
            or field_type == FieldType.longstr
            or field_type == FieldType.table
        ):
            return ""
        else:
            return 0

    def handle_content_header_packet(self, content_header_packets):
        """
        Content header packets have core fields body_size, property_flags,
        + a list of optional fields based on class.

        The parsed xml's field name is matched with the wire shark field name.
        If there is no matching value in wireshark, we use the default value.
        """
        content_hander_tests = []
        for class_id, content_header_packet in content_header_packets.items():
            (content_header_details, content_header_value) = content_header_packet
            amqp_class = self.amqp_clas_id_class[str(class_id)]
            frame_type = content_header_details.get_field_value("type")
            channel = content_header_details.get_field_value("channel")
            payload_size = content_header_details.get_field_value("Length")
            header_body_size = int(
                content_header_details.get_field_value("header_body_size")
            )
            header_property_flags = int(
                content_header_details.get_field_value("header_property_flags"), 16
            )

            message_body = OrderedDict({})
            message_body["body_size"] = header_body_size
            message_body["property_flags"] = header_property_flags
            for field in amqp_class.content_header_method.fields:
                field_name = field.c_field_name
                pcap_field_name = f"method_properties_{field_name}"
                if field.field_type == FieldType.table:
                    continue

                if field_name in message_body:
                    continue
                if pcap_field_name in content_header_details.field_names:
                    message_body[field_name] = content_header_details.get_field_value(
                        pcap_field_name
                    )
                else:
                    message_body[field_name] = self.get_default_value_field(
                        field.field_type
                    )

            msg_body = json.dumps(message_body, separators=(",", ":")).replace(
                '"', '\\"'
            )
            content_hander_tests.append(
                self.gen_test(
                    test_name=f"content_header_{amqp_class.class_name.lower()}",
                    raw_packet_value=content_header_value,
                    msg=f"frame_type=[{frame_type}] channel=[{channel}] payload_size=[{payload_size}] msg=[{msg_body}]",
                )
            )
        return content_hander_tests

    def dic_to_msg_body(self, dic):
        """
        Converts the dictionary {} to an escaped json string
        """
        return json.dumps(dic, separators=(",", ":")).replace('"', '\\"')

    def generate_method(self, method_packets):
        """
        Converts all ContentMethod Frame types to a list of tuples for a google test.

        The parsed xml's field name is matched with the wireshark field name.
        If there is no matching value in wireshark, we use the default value.
        """
        amqp_methods_gen = []
        for amqp_key, packet in method_packets.items():
            amqp_method: AMQPMethod = self.amqp_class_id_method_id_method[amqp_key]
            (packet_details, packet_value) = packet
            frame_type = packet_details.get_field_value("type")
            channel = packet_details.get_field_value("channel")
            payload_size = packet_details.get_field_value("Length")
            message_body = {}
            for field in amqp_method.fields:
                field_name = field.c_field_name
                pcap_field_name = f"method_arguments_{field_name}"
                if field.field_type == FieldType.table:
                    continue
                default_value = self.get_default_value_field(field.field_type)
                if pcap_field_name in packet_details.field_names:
                    message_body[field_name] = packet_details.get_field_value(
                        pcap_field_name
                    )
                    if isinstance(default_value, int):
                        message_body[field_name] = int(message_body[field_name])
                else:
                    message_body[field_name] = default_value

            msg_body = self.dic_to_msg_body(message_body)
            santized_method_name = amqp_method.method_name.lower().replace("-", "_")
            test_name = (
                f"amqp_method_{amqp_method.class_name.lower()}_{santized_method_name}"
            )
            msg = f"frame_type=[{frame_type}] channel=[{channel}] payload_size=[{payload_size}] msg=[{msg_body}]"
            amqp_methods_gen.append(
                self.gen_test(
                    test_name=test_name,
                    raw_packet_value=packet_value,
                    msg=msg,
                )
            )
        return amqp_methods_gen

    def generate_test_code(self):
        """
        Generates the test code for all the frame types via the jinja template.
        """
        with open(self.unique_packet_data_path, "rb") as f:
            unique_packet_data: UniquePacketData = dill.load(f)
        heartbeat_test = self.gen_heartbeat_test(unique_packet_data.heartbeat_packet)
        content_body_tests = self.gen_content_body_test(
            unique_packet_data.content_body_packets
        )
        content_header_tests = "".join(
            self.handle_content_header_packet(unique_packet_data.content_header_packets)
        )
        method_tests = "".join(self.generate_method(unique_packet_data.method_packets))

        template = self.env.get_template("parse_decode_test.cc.jinja_template")

        with self.parse_decode_test_gen_path.open("w") as f:
            f.write(
                template.render(
                    heartbeat_test=heartbeat_test,
                    content_body_tests=content_body_tests,
                    content_header_tests=content_header_tests,
                    amqp_method_tests=method_tests,
                )
            )

    def run(self):
        print("Generating Code...")
        self.generate_test_code()
        print("Formating code...")
        self.format()

    def format(self):
        if input("Use clang-format to format code[y/n]") != "y":
            return

        p = subprocess.Popen(
            [
                "clang-format",
                "-style=Google",
                "-i",
                str(self.parse_decode_test_gen_path),
            ]
        )
        p.wait()


if __name__ == "__main__":
    code_generator = CodeTestGenerator()
    fire.Fire(
        {
            "parse_packets": code_generator.parse_packets,
            "run": code_generator.run,
        }
    )
