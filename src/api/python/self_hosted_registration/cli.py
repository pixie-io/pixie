#!/usr/bin/env python3
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

r"""
Pixie registration CLI

This module provides a script for onboarding Pixie Orgs and generating deployment and
API keys for self-hosted Pixie cloud installations. This serves as preliminary step to
Pixie Org deployment and bootstraps usage of the (more advanced) Pixie API client using
gRPC/protobufs.

Example for registering a new Pixie Org:

    $ self_hosted_registration/cli.py \
            --verbose \
            --insecure \
            --pixie-url 'https://work.dev.withpixie.dev' \
            --hydra-user-url https://work.dev.withpixie.dev/oauth/hydra \
        register-pixie-org \
            --pixie-org my_test_org

    Pixie Org created: my_test_org
    OAuth client-secret: <oauth-client-secret>
    Administrator identity ID: <administrator-identity-id>
    Administrator username: admin@my_test_org
    Administrator password: <administrator-password>
    API key ID: <api-key-id>
    API key secret: <api-key-secret>

Example for registering a new Deployment Secret (for deploying the Vizier sensor) on
a previously registered Pixie Org (using its identity ID):

    $ self_hosted_registration/cli.py \
            --verbose \
            --insecure \
            --pixie-url 'https://work.dev.withpixie.dev' \
            --hydra-user-url https://work.dev.withpixie.dev/oauth/hydra \
        add-deployment-key \
            --identity-id <administrator-identity-id>
    Please provide client-secret: <oauth-client-secret>

    Deployment-key ID: <deployment-key-id>
    Deployment-key secret: <deployment-key-secret>

NOTE: In all examples, we assume the parent directory of this script has been added to
the PYTHONPATH.

For a list of available commands, use

    $ self_hosted_registration/cli.py --help

For a list of available options for each command, use

    $ self_hosted_registration/cli.py <command> --help
"""

import argparse
import enum
import getpass
import http
import http.client
import json
import logging
import random
import string
import sys
import typing
import yaml

import self_hosted_registration
from self_hosted_registration import registration


class Error(self_hosted_registration.Error):
    """Base-class for exceptions in this module"""


class EnumWithFromValue(enum.Enum):
    """Wrapper around `enum.Enum` that supports `from_value()`"""

    def __str__(self) -> str:
        return self.value

    @classmethod
    def from_value(cls, value: str):
        """
        Look up enum element by its value

        NOTE: Needed for usage in `argparse`

        :param value: The value of the component
        :return: An enum element
        :rtype: cls
        :raise: ValueError
        """
        for element in cls:
            if element.value == value:
                return element
        raise ValueError(value)


@enum.unique
class ScriptCommand(EnumWithFromValue):
    """The "commands" supported by this script"""

    REGISTER_PIXIE_ORG = "register-pixie-org"
    DELETE_IDENTITY = "delete-identity"
    ADD_DEPLOYMENT_KEY = "add-deployment-key"


@enum.unique
class ScriptOutputFormat(EnumWithFromValue):
    """The "output formats" supported by this script"""

    TEXT = "text"
    YAML = "yaml"
    JSON = "json"


def get_random_password(
    password_chars: str = string.ascii_letters + string.digits,
    password_length: int = 32,
) -> str:
    """
    Get a random password

    :param password_chars: String from which to choose password characters
    :param password_length: Length of the password
    :return: Password string
    """
    return "".join(random.choices(password_chars, k=password_length))


def get_password_from_file_or_stdin(
    filename: typing.Optional[str],
    password_description: str,
    strip_whitespace: bool = True,
) -> str:
    """
    Get a password from file

    :param filename: File to read; if `None`, read from stdin
    :param password_description: Description of the password we are reading
    :param strip_whitespace: If `True`, strip leading and trailing whitespaces from the password.
        Useful for passwords from file containing newlines
    :return: Password string
    :raise Error: Reading failed
    """
    if filename is None:
        secret_value = getpass.getpass(
            prompt=f"Please provide {password_description}: "
        )
    else:
        try:
            with open(filename, mode="rt", encoding="UTF-8") as secret_file:
                secret_value = secret_file.read()
        except IOError as err:
            raise Error(
                f"Reading {password_description} from {filename} failed: {err}"
            ) from err
    if strip_whitespace:
        secret_value = secret_value.strip()
    if not secret_value:
        raise Error(f"Invalid {password_description}: cannot be empty")
    return secret_value


def parse_cli() -> typing.Tuple[argparse.ArgumentParser, argparse.Namespace]:
    """Helper to get the CLI parser"""
    parser = argparse.ArgumentParser(
        usage="""
===============================================================================
"""
        + __doc__.strip()
        + """

===============================================================================

USAGE: %(prog)s [options]

"""
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help="Enable verbose logging (use multiple times for even more verbose logging). NOTE: "
        "Very very verbose (-vvv) logging enables emitting debug logs of the API responses, which "
        "contains session tokens. Very very very verbose (-vvvv) logging will emit raw HTTP "
        "requests and will contain all contents, including passwords. Do NOT use in production "
        "environments if outputs are logged",
    )
    for api in registration.ApiRequestHelper.KNOWN_APIS:
        api_name = api.name.value

        def flag_name(option):
            # pylint: disable=cell-var-from-loop
            return f"--{api_name.lower().replace(' ', '-')}-{option}"

        parser.add_argument(
            flag_name("url"),
            default=api.default_base_url,
            help=f"Base path to the {api_name} API (default: %(default)s).",
        )
        flag_name_insecure = flag_name("insecure")
        parser.add_argument(
            flag_name_insecure,
            action="store_true",
            help=f"Do not validate SSL connections to {api_name}.",
        )
        parser.add_argument(
            flag_name("verify"),
            required=False,
            help="Optional path to certificate bundle for validating SSL connections "
            + f"to {api_name}. Ignored if using {flag_name_insecure}",
        )
    # add a "global insecure" flag just for convenience of use
    parser.add_argument(
        "-k",
        "--insecure",
        action="store_true",
        help="Do not validate SSL connections to any API.",
    )
    parser.add_argument(
        "--output",
        type=ScriptOutputFormat,
        default=ScriptOutputFormat.TEXT.value,
        help="Format of the output to produce (choose from "
        f"{', '.join(str(output.value) for output in ScriptOutputFormat)}; default: %(default)s).",
    )

    subparsers = parser.add_subparsers(
        dest="command",
        # NOTE: `add_subparsers(required=True)` was added only in Python 3.7, and we
        # don't want to break older python versions
        # required=True,
    )

    register_org_parser = subparsers.add_parser(
        ScriptCommand.REGISTER_PIXIE_ORG.value, help="Register a new Pixie Org."
    )
    register_org_parser.add_argument(
        "--pixie-org",
        required=True,
        type=str,
        help="Name of the Pixie Org to register (must match "
        f"{registration.PixieRegistrationApiClient.PIXIE_ORG_NAME_REGEXP.pattern}).",
    )
    register_org_parser.add_argument(
        "--client-secret",
        type=str,
        default=None,
        # NOTE: We don't provide a "--from-file" to double-down on encouraging the user to not
        # specify the secret and let the script auto-generate it
        help="Optional client secret for creating the Machine-To-Machine OAuth client. If not "
        "provided, a random secret is generated. This secret must be stored securely for later "
        "automation, do not give to users. NOTE: It is strongly discouraged to specify a "
        "secret; instead, let the script auto-generate a strong secret.",
    )
    register_org_parser.add_argument(
        "--admin-email-address",
        type=str,
        default=None,
        help="Optional email address to use for the administrative account. If not provided, "
        "it default to admin@<pixie-org>.",
    )
    register_org_parser.add_argument(
        "--admin-password",
        type=str,
        default=None,
        # NOTE: We don't provide a "--from-file" to double-down on encouraging the user to not
        # specify the password and let the script auto-generate it
        help="Optional password for the administrative account. If not provided, a random "
        "password is generated. NOTE: It is strongly discouraged to specify a password; instead, "
        "let the script auto-generate a strong password.",
    )
    register_org_parser.add_argument(
        "--skip-pixie-api-key-creation",
        dest="create_pixie_api_key",
        action="store_false",
        help="Skip creation of the Pixie API key.",
    )

    delete_identity_parser = subparsers.add_parser(
        ScriptCommand.DELETE_IDENTITY.value,
        help="Delete an identity. NOTE: This will not delete the Pixie Org, as this is "
        "currently not exposed via APIs.",
    )
    delete_identity_parser.add_argument(
        "--identity-id",
        type=str,
        required=True,
        help="ID of the administrative account of the Pixie Org to delete.",
    )

    add_deployment_parser = subparsers.add_parser(
        ScriptCommand.ADD_DEPLOYMENT_KEY.value,
        help="Add a new Vizier deployment-key.",
    )
    add_deployment_parser.add_argument(
        "--identity-id",
        type=str,
        required=True,
        help="ID of the administrative account of the Pixie Org to use.",
    )
    add_deployment_parser.add_argument(
        "--client-secret",
        type=str,
        default=None,
        help="Use this client secret for authenticating the Machine-To-Machine OAuth "
        "client (instead of reading from file or stdin).",
    )
    add_deployment_parser.add_argument(
        "--client-secret-from-file",
        type=str,
        default="-",
        help="Read the client secret for authenticating the Machine-To-Machine OAuth client from "
        'this file; use "-" to read from stdin.',
    )
    args = parser.parse_args()

    if not args.command:  # see comment above - we don't want to mark it as required
        choices = ",".join(str(command) for command in ScriptCommand)
        parser.error(f"Need type of command to run. Choose from {choices}")
    # seems we cannot assign an enum type for sub-parsers, so we have to restore type-safety here
    args.command = ScriptCommand.from_value(args.command)

    # propagate the "global insecure" flag so we don't have to deal with it all over the place
    if args.insecure:
        for api in registration.ApiRequestHelper.KNOWN_APIS:
            vars(args)[api.get_option_name("insecure")] = True

    if args.command == ScriptCommand.REGISTER_PIXIE_ORG:
        if not registration.PixieRegistrationApiClient.PIXIE_ORG_NAME_REGEXP.match(
            args.pixie_org
        ):
            parser.error(f"Invalid Pixie Org: {args.pixie_org}")

        if not args.client_secret:
            args.client_secret = get_random_password()

        if not args.admin_email_address:
            args.admin_email_address = f"admin@{args.pixie_org}"

        if not args.admin_password:
            args.admin_password = get_random_password()

    if args.command == ScriptCommand.ADD_DEPLOYMENT_KEY:
        if not args.client_secret:
            try:
                args.client_secret = get_password_from_file_or_stdin(
                    filename=None
                    if args.client_secret_from_file == "-"
                    else args.client_secret_from_file,
                    password_description="client-secret",
                )
            except Error as err:
                parser.error(err)

    return parser, args


def do_register_org(
    pixie_org: str,
    client_secret: str,
    admin_email_address: str,
    admin_password: str,
    identity_admin_api_client: registration.IdentityAdminApiClient,
    oauth_api_client: registration.OAuthApiClient,
    pixie_api_client: registration.PixieRegistrationApiClient,
    create_pixie_api_key: bool = True,
    output_format: ScriptOutputFormat = ScriptOutputFormat.TEXT,
) -> None:
    """
    Do "register-pixie-org" workflow

    The Pixie Org registration work-flow does the following steps:
    - create a new identity in Kratos for authentication (using email+password)
    - create a new OAuth client in Hydra for authenticating the newly created identity. The
      ID of the Kratos identity is used to identify this client
    - create a Pixie Org, linked to the created identity, and initialize Pixie Org settings

    :param pixie_org: Name of the Pixie Org to create
    :param client_secret: Client secret for authenticating the Machine-To-Machine OAuth client
    :param admin_email_address: Email address of the administrative user
    :param admin_password: Administrative user password
    :param identity_admin_api_client: API client for the identity APIs
    :param oauth_api_client: API client for the oAuth API
    :param PixieRegistrationApiClient: API client for the Pixie API
    :param create_pixie_api_key: If `True`, create a default Pixie API key
    :param output_format: Format output to use
    """
    logger = logging.getLogger("do_register_org")

    logger.info("Creating identity: %s", admin_email_address)
    identity_id = identity_admin_api_client.create_identity(
        client_secret=client_secret,
        email_address=admin_email_address,
        password=admin_password,
    )
    logger.debug("Authenticating identity: %s", identity_id)
    access_token = oauth_api_client.get_access_token(
        username=identity_id,
        password=client_secret,
    )
    logger.info("Performing Pixie Org signup: %s", pixie_org)
    pixie_api_client.signup(
        org_name=pixie_org,
        access_token=access_token,
    )

    logger.debug("Performing Pixie API login")
    pixie_api_client.login(access_token=access_token)
    logger.info("Configuring Pixie Org settings: make user approvals a requirement")
    pixie_api_client.configure_org_settings(enable_user_approvals=True)
    logger.info("Configuring Pixie user settings: opt-out of UI analytics")
    pixie_api_client.configure_user_settings(enable_analytics=False)

    api_key_id, api_key_secret = None, None
    if create_pixie_api_key:
        logger.info("Creating Pixie API key")
        api_key_id, api_key_secret = pixie_api_client.create_api_key()

    logger.info("Pixie registration successful")
    if output_format == ScriptOutputFormat.TEXT:
        print(f"Pixie Org created: {pixie_org}")
        print(f"OAuth client-secret: {client_secret}")
        print(f"Administrator identity ID: {identity_id}")
        print(f"Administrator username: {admin_email_address}")
        print(f"Administrator password: {admin_password}")
        if create_pixie_api_key:
            print(f"API key ID: {api_key_id}")
            print(f"API key secret: {api_key_secret}")
    else:
        output = {
            "pixie_org": {
                "name": pixie_org,
            },
            "admin_account": {
                "id": identity_id,
                "client_secret": client_secret,
                "email_address": admin_email_address,
                "password": admin_password,
            },
            "pixie_api": {
                "temp_access_token": access_token,
            },
        }
        if create_pixie_api_key:
            output["pixie_api"]["api_key"] = {
                "id": api_key_id,
                "secret": api_key_secret,
            }
        if output_format == ScriptOutputFormat.JSON:
            print(json.dumps(output, indent="    "))
        elif output_format == ScriptOutputFormat.YAML:
            print(yaml.safe_dump(output))


def do_delete_identity(
    identity_id: str,
    identity_admin_api_client: registration.IdentityAdminApiClient,
    output_format: ScriptOutputFormat = ScriptOutputFormat.TEXT,
) -> None:
    """
    Do "delete-identity" workflow

    :param identity_id: ID of the administrative user identity to delete
    :param identity_admin_api_client: API client for the identity APIs
    :param output_format: Format output to use
    """
    logger = logging.getLogger("do_delete_identity")

    logger.info("Deleting identity: %s", identity_id)
    # NOTE: Pixie has no API for doing this deletion, so this may cause issues later if the
    # Pixie Org already exists
    deleted = identity_admin_api_client.delete_identity(
        identity_id=identity_id,
    )
    if deleted:
        logger.info("Identity deletion successful")
    else:
        logger.info("Identity deletion skipped: identity not found")

    if output_format == ScriptOutputFormat.TEXT:
        print(f"Identity {'' if deleted else 'not '}deleted")
    else:
        output = {"deleted": deleted}
        if output_format == ScriptOutputFormat.JSON:
            print(json.dumps(output, indent="    "))
        elif output_format == ScriptOutputFormat.YAML:
            print(yaml.safe_dump(output))


def do_add_deployment(
    identity_id: str,
    client_secret: str,
    oauth_api_client: registration.OAuthApiClient,
    pixie_api_client: registration.PixieRegistrationApiClient,
    output_format: ScriptOutputFormat = ScriptOutputFormat.TEXT,
) -> None:
    """
    Do "add-deployment" workflow

    :param identity_id: ID of the administrative user identity
    :param client_secret: Client secret for authenticating the Machine-To-Machine OAuth client
    :param oauth_api_client: API client for the oAuth API
    :param PixieRegistrationApiClient: API client for the Pixie API
    :param output_format: Format output to use
    """
    logger = logging.getLogger("do_add_deployment")

    logger.debug("Authenticating identity: %s", identity_id)
    access_token = oauth_api_client.get_access_token(
        username=identity_id,
        password=client_secret,
    )

    logger.debug("Performing Pixie API login")
    pixie_api_client.login(access_token=access_token)

    logger.debug("Creating Pixie deployment-key")
    (
        deployment_key_id,
        deployment_key_secret,
    ) = pixie_api_client.create_deployment_key()
    logger.debug("Created Pixie deployment-key: %s", deployment_key_id)
    if output_format == ScriptOutputFormat.TEXT:
        print(f"Deployment-key ID: {deployment_key_id}")
        print(f"Deployment-key secret: {deployment_key_secret}")
    else:
        output = {
            "deployment_key_id": deployment_key_id,
            "deployment_key_secret": deployment_key_secret,
        }
        if output_format == ScriptOutputFormat.JSON:
            print(json.dumps(output, indent="    "))
        elif output_format == ScriptOutputFormat.YAML:
            print(yaml.safe_dump(output))


def main():
    """Script entrypoint"""
    _parser, args = parse_cli()
    logging.basicConfig(
        level=logging.DEBUG
        if args.verbose > 1
        else logging.INFO
        if args.verbose
        else logging.WARNING
    )
    logger = logging.getLogger("main")

    if args.verbose > 3:  # enable tracing in requests in very very very verbose mode
        http.client.HTTPConnection.debuglevel = 1

    def get_identity_admin_api_client():
        return registration.IdentityAdminApiClient(
            kratos_api_client=registration.ApiRequestHelper.factory_by_known_api_name_from_cli(
                api_name=registration.KnownApiName.KRATOS_ADMIN, args=args
            ),
            hydra_api_client=registration.ApiRequestHelper.factory_by_known_api_name_from_cli(
                api_name=registration.KnownApiName.HYDRA_ADMIN, args=args
            ),
        )

    def get_oauth_api_client():
        return registration.OAuthApiClient(
            api_client=registration.ApiRequestHelper.factory_by_known_api_name_from_cli(
                api_name=registration.KnownApiName.HYDRA_USER, args=args
            ),
        )

    def get_pixie_api_client():
        return registration.PixieRegistrationApiClient(
            api_client=registration.ApiRequestHelper.factory_by_known_api_name_from_cli(
                api_name=registration.KnownApiName.PIXIE, args=args
            ),
        )

    try:
        if args.command == ScriptCommand.REGISTER_PIXIE_ORG:
            do_register_org(
                pixie_org=args.pixie_org,
                client_secret=args.client_secret,
                admin_email_address=args.admin_email_address,
                admin_password=args.admin_password,
                identity_admin_api_client=get_identity_admin_api_client(),
                oauth_api_client=get_oauth_api_client(),
                pixie_api_client=get_pixie_api_client(),
                create_pixie_api_key=args.create_pixie_api_key,
                output_format=args.output,
            )
        elif args.command == ScriptCommand.DELETE_IDENTITY:
            do_delete_identity(
                identity_id=args.identity_id,
                identity_admin_api_client=get_identity_admin_api_client(),
                output_format=args.output,
            )
        elif args.command == ScriptCommand.ADD_DEPLOYMENT_KEY:
            do_add_deployment(
                identity_id=args.identity_id,
                client_secret=args.client_secret,
                oauth_api_client=get_oauth_api_client(),
                pixie_api_client=get_pixie_api_client(),
                output_format=args.output,
            )
    except self_hosted_registration.Error as err:
        logger.error("Invocation failed: %s", err)
        if args.verbose > 3:
            raise
    except Exception as err:
        logger.error("Unexpected error while processing request: %s", err)
        raise

    return 0


if __name__ == "__main__":
    sys.exit(main())
