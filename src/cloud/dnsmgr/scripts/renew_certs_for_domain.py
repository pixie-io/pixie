import argparse
import os
import shutil
import json
import subprocess
import filecmp


def parse_args():
    parser = argparse.ArgumentParser(
        description='Renew all of the certs')
    parser.add_argument(
        'parent_domain', help='Parent domain of the certificates to run through.')
    parser.add_argument('original_certs_dir',
                        help='The original certificates directory.')
    parser.add_argument(
        'new_certs_dir', help='The new certificates directory.')
    parser.add_argument(
        'email', help='Email to renew for the certificates.')
    parser.add_argument(
        '--lego', help='lego binary to use', default='./lego')
    parser.add_argument('--create_new_certs_dir',
                        help='Whether to create the new certs dir or not', action='store_true')
    parser.add_argument(
        '--cname', help='specific cname to run. Cannot be used with --renew_all', default=None)
    parser.add_argument('--renew_all',
                        help='Whether to renew all certs instead of the unchanged.'
                        'Cannot be used with --cname',
                        action='store_true')
    parser.add_argument('--num_tries',
                        help='Number of tries to make for each cert.', default=3)
    return parser.parse_args()


def backup_dir(d):
    assert os.path.exists(d)
    backup_name = d + '_backup'
    name_try = 0
    while os.path.exists(backup_name):
        backup_name = d + '_backup{}'.format(name_try)
        name_try += 1
    shutil.copytree(d, backup_name)
    return backup_name


def delete_dir(d):
    assert os.path.exists(d)
    shutil.rmtree(d)


def get_same_files_in_dcmp(dcmp):
    ''' Returns all the same files in the diff comparison object '''
    same_files = list(dcmp.same_files)
    for sub_dcmp in dcmp.subdirs.values():
        same_files.extend(get_same_files_in_dcmp(sub_dcmp))
    return same_files


def get_same_files(old_dir, new_dir):
    ''' Gets the files that are the same in the old and new directory '''
    dcmp = filecmp.dircmp(old_dir, new_dir)
    return get_same_files_in_dcmp(dcmp)


def cert_fname_to_domain(fname):
    '''
    Converts a filename for a certificate to the domain that the cert
    satisfies.
    '''
    assert fname.startswith('_'), '{} must start with "_"'.format(fname)
    assert fname.find(
        '_', 1) == -1, '{} must not contain "_" other than in the first character'.format(fname)
    return fname.replace('_', '*')


def get_domains_in_cert_files(files):
    '''
    Gets the domains for all cert_files passed in.
    '''
    cert_domains = []
    for d in files:
        if not d.endswith('.key'):
            continue
        d = d.replace('.key', '')
        cert_domains.append(cert_fname_to_domain(d))
    return set(cert_domains)


def get_cert_domains(lego_dir, parent_domain):
    '''
    Returns the domains that have certificates
    in the lego certificate directory.
    '''
    cert_domains = []
    certs_dir = os.path.join(lego_dir, 'certificates')
    assert os.path.exists(certs_dir)
    for d in get_domains_in_cert_files(os.listdir(certs_dir)):
        # Make sure that domain should be added.
        if parent_domain not in d:
            continue
        cert_domains.append(d)

    return cert_domains


def run_and_retry(cmd, num_tries=1):
    '''
    Run a command and retry `num_tries` times.
    '''
    res = None
    for i in range(num_tries):
        res = subprocess.call(cmd, shell=True)
        if not res:
            break
    return res


def renew_lego_cert(domain, out_dir, email, lego, num_tries):
    '''
    Function that wraps the lego cmd to renew the lego certificate.
    '''
    cmd = '{lego} --email="{email}" --domains="{domain}" ' \
        '--dns="gcloud" --path="{out_dir}" -k rsa4096 -a run'.format(
            lego=lego,
            email=email,
            domain=domain,
            out_dir=out_dir,
        )
    print('\n------')
    print('Running {}'.format(domain))
    return run_and_retry(cmd, num_tries)


def renew_all_certs(domains, out_dir, email, lego, num_tries):
    '''
    Renews all of the certificates for the domains passed in.
    '''
    failed_certs = []
    for d in list(domains):
        domains.remove(d)
        res = renew_lego_cert(d, out_dir, email, lego, num_tries)
        # TODO(philkuz) this doesn't actually append the right things.
        # Was able to successfully update creds without problems
        if not res:
            failed_certs.append(d)
    return failed_certs


def prepare_new_cert_dir(original_certs_dir, new_certs_dir, create_new_certs_dir):
    '''
    Checks to make sure that the original_certs_dir and
    new_certs_dir are properly setup and potentially create
    a new cert_dir at `new_certs_dir` if
    `create_new_certs_dir == True`.
    '''
    # Make sure the original out directory is not the same as the new out directory.
    assert original_certs_dir != new_certs_dir

    accounts_dir = os.path.join(original_certs_dir, 'accounts')
    assert os.path.exists(accounts_dir)

    if create_new_certs_dir:
        assert not os.path.exists(new_certs_dir)
        shutil.copytree(original_certs_dir, new_certs_dir)

    new_accounts_dir = os.path.join(new_certs_dir, 'accounts')
    assert os.path.exists(new_accounts_dir)


def get_unrenewed_domains(old_dir, new_dir, parent_domain):
    '''
    Get the domains that have not been renewed by comparing the original cert dir
    with the new directory. Any cert that's the same between the two files
    has yet to be updated.
    '''

    cert_files = get_same_files(os.path.join(
        old_dir, 'certificates'), os.path.join(new_dir, 'certificates'))
    return [d for d in get_domains_in_cert_files(
            cert_files) if parent_domain in d]


def main():
    # Backup the original out directory.
    args = parse_args()

    if args.renew_all and args.cname:
        raise ValueError(
            '--renew_all and --cname cannot be used at the same time.')

    if args.renew_all:
        # Grab the list of certs from the original out directory.
        cert_domains = get_cert_domains(
            args.original_certs_dir, args.parent_domain)
    elif args.cname:
        # Filter only for matching cert names.
        cert_domains = get_cert_domains(
            args.original_certs_dir, args.parent_domain)
        cert_domains = [c for c in cert_domains if args.cname in c]
    else:
        print("only doing unrenewed domains")
        cert_domains = get_unrenewed_domains(
            args.original_certs_dir, args.new_certs_dir, args.parent_domain)

    print("renewing cert domains {}".format(len(cert_domains)))
    print(cert_domains)

    backup_dir_name = backup_dir(args.original_certs_dir)
    print("Backed up original {} at {}".format(
        args.original_certs_dir, backup_dir_name))

    # Prepare the new certs directory.
    prepare_new_cert_dir(args.original_certs_dir,
                         args.new_certs_dir, args.create_new_certs_dir)

    remaining_certs = set(cert_domains)
    print('running {} certs updates'.format(len(remaining_certs)))

    # Iterate and renew each cert_domain.
    failed_certs = renew_all_certs(remaining_certs, args.new_certs_dir,
                                   args.email, args.lego, args.num_tries)
    if failed_certs:
        print(json.dumps(failed_certs, indent=4))
        print('number of failed certs {}'.format(
            len(failed_certs)))

    # Clean up backup.
    print("deleting backup at {}".format(backup_dir_name))
    delete_dir(backup_dir_name)


if __name__ == "__main__":
    main()
