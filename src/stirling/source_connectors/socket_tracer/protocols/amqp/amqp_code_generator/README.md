# AMQP Protocol Parsing

The function takes in the `amqp-0-9-1.stripped.xml` from the AMQP specification and generates header and c files for pixie.
The stripped version of xml from https://www.amqp.org/specification/0-9-1/amqp-org-download

Bazel cmds:
```
  wget "https://www.rabbitmq.com/resources/specs/amqp0-9-1.xml"
  bazel run //src/stirling/source_connectors/socket_tracer/protocols/amqp/amqp_code_generator:amqp_code_gen_main -- run
  cp generated_files/{decode.h,decode.cc,types_gen.h} ../
  cp generated_files/amqp.h src/carnot/funcs/protocols/amqp.h
  arc lint
```


The code generation process involves 3 steps:
1. Converting the xml to python objects
2. Generating the strings of all code generation peices
3. Generating the output files(with headers/licenses information)

## Frame Structure
The AMQP structure is split up into 4 types of frames: Method, Header, Body, and Heartbeat.

### Heartbeat Frame
The Heartbeat frame is unique and consists of (frame type, channel, Length)

### Method Frame
The method frame represents an AMQP method such as Publish/Deliver/Ack.
The structure of a method frame consists of (frame type, channel, class id, method id). Depending on the class id and method id, the frame can consist of different arguments

### Content Header Frame
Each AMQP class has a relevant content header frame with different arguments. This usually precedes a Content Body and describes what will be sent.
The structure of a content header frame consists of (frame type, channel, length, class_id, weight, body_size, property_flags, property list)

The content header has special property flags 0xNNNN that dynamically determine if an argument in the property list will show up. For example, if bit 3 is set, the property list will contain the third argument of the class.

### Content Body
These represent the general content body packets that are sent. The structure of a content body frame consists of (frame type, channel, length, body)


## XML structure
The XML holds relevant fields, method ids, and class ids. A sample snippet parsed is below.
```
<amqp major="0" minor="9" revision="1" port="5672">
<class name="connection" handler="connection" index="10">
    <method name="start" synchronous="1" index="10">
      <chassis name="client" implement="MUST"/>
      <response name="start-ok"/>
      <field name="version-major" domain="octet"/>
```


## AMQP Test generation
Since AMQP has many methods and classes, in `amqp_test_code_gen.py` a sample wireshark capture of AMQP traffic can be used to test the decoding of the different fields.

This is done via:
```
bazel run //src/stirling/source_connectors/socket_tracer/protocols/amqp/amqp_code_generator:amqp_test_code_gen -- parse_packets

bazel run //src/stirling/source_connectors/socket_tracer/protocols/amqp/amqp_code_generator:amqp_test_code_gen -- run

cp generated_files/parse_decode_test.cc .
```

Note: There are some fields that are failed to be parsed by wireshark. These fields need to be manually edited when testing the wireshark code.
Some properties such as:
- Exchange key
- Content Type
