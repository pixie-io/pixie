# AMQP Protocol Parsing

Install the requirements via pip install and run
```bash
    python3 amqp_decode_gen.py run
```

The function takes in the `amqp-0-9-1.stripped.xml` from the AMQP specification and generates header and c files for pixie.

The code generation process involes 3 steps:
1. Converting the xml to python objects
2. Generating the strings of all code generation peices
3. Generating the output files(with headers/licenses information)
