import yaml
import sys
import base64

# Takes a k8s secret yaml from stdout, and base64 decodes the value at each field,
# writing the result back to stdout. Used for service_tls_certs.


def str_presenter(dumper, data):
    if len(data.splitlines()) > 1:  # check for multiline string
        return dumper.represent_scalar('tag:yaml.org,2002:str', data, style='|')
    return dumper.represent_scalar('tag:yaml.org,2002:str', data)


def main():
    lines = ""
    for line in sys.stdin:
        lines += line

    data = yaml.load(lines)
    data['stringData'] = {}
    for k, v in data['data'].items():
        data['stringData'][k] = base64.b64decode(v)

    del data['data']

    metadata_keys = list(data['metadata'])
    for mk in metadata_keys:
        if mk != "name":
            del data['metadata'][mk]

    yaml.add_representer(str, str_presenter)
    yaml.dump(data, sys.stdout)


if __name__ == '__main__':
    main()
