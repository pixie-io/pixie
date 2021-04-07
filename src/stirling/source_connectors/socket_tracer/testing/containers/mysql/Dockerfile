FROM python:3.10.0a6-alpine3.12

RUN pip install --trusted-host pypi.python.org -q mysql-connector-python

# A location for storing scripts.
RUN mkdir /scripts

# Give python a startup script that prints the python interpreter pid.
# Used by the tests to filter by pid when ensuring that the MySQL messages were traced.
RUN mkdir -p /root/.local/lib/python3.10/site-packages
RUN echo "import os" > /root/.local/lib/python3.10/site-packages/sitecustomize.py
RUN echo "print(\"pid=\" + str(os.getpid()))" >> /root/.local/lib/python3.10/site-packages/sitecustomize.py

# Entrypoint is python. Accepts a script as an argument.
ENTRYPOINT ["python"]

