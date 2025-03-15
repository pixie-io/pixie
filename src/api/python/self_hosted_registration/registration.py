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

# Pixie registration helper code
#
# This module provides helper classes for onboarding Pixie Orgs and generating deployment and
# API keys. This serves as preliminary step to Org deployment and bootstraps usage of the (more
# advanced) Pixie API client using gRPC/protobufs.
#
# Pixie uses the Ory system for identity management (a combination of the Kratos and Hydra APIs)
# which this script deals with. It allows creating an Ory identity that can be used to authenticate
# to the Pixie APIs and bootstrap the Pixie Org deployment.
#
# NOTE: Pixie uses the OAuth workflow for logins to obtain an `access_token`, which can then be
# used to obtain an API token for interacting with the actual Pixie API. Internally it uses
# Kratos (see https://www.ory.sh/docs/kratos/reference/api) for Identity management and
# Hydra (see https://www.ory.sh/docs/hydra/reference/api) for the actual OAuth2 workflow
# handling.

import argparse
import enum
import http
import http.client
import logging
import os
import pprint
import re
import typing
import urllib.parse

import requests

import self_hosted_registration


# Pylint has a point, but we want to keep this module in a single file, as we are sharing it with
# the OSS community
#
# pylint: disable=too-many-lines


class Error(self_hosted_registration.Error):
    """Base-class for exceptions in this module"""


class ApiError(Error):
    """Class for handling API request errors"""


@enum.unique
class KnownApiName(enum.Enum):
    """Listing of known APIs"""

    PIXIE = "Pixie"
    KRATOS_ADMIN = "Kratos Admin"
    HYDRA_ADMIN = "Hydra Admin"
    HYDRA_USER = "Hydra User"


class KnownApi(typing.NamedTuple):
    """Helper for declaring known APIs"""

    name: KnownApiName
    default_base_url: str

    def get_option_name(self, name: str) -> str:
        """
        Helper to get a standardized name for the given option specific to this API

        :param name: Name of the option
        :return: The <api>_<name> name for the given option for this API
        """
        return f"{self.name.value.lower().replace(' ', '_')}_{name}"


class ApiRequestHelper:
    """
    Class for issuing API requests

    NOTE: This is merely a thin wrapper around `requests` to reduce code-duplication
    """

    KNOWN_APIS = (
        KnownApi(
            name=KnownApiName.KRATOS_ADMIN,
            # NOTE: This service/URL is not exposed in k8s ingress (rightfully so!). To access it,
            # the user must run this code within the k8s cluster or tunnel to the service IP
            default_base_url="https://kratos.plc.svc:4434",
        ),
        KnownApi(
            name=KnownApiName.HYDRA_ADMIN,
            # NOTE: This service/URL is not exposed in k8s ingress (rightfully so!). To access it,
            # the user must run this code within the k8s cluster or tunnel to the service IP
            default_base_url="https://hydra.plc.svc:4445",
        ),
        KnownApi(
            name=KnownApiName.HYDRA_USER,
            # NOTE: We use the URL exposed via k8s ingress. This API is also exposed internally
            # using the following internal service name:
            #
            #     default_base_url="https://hydra.plc.svc:4444",
            #
            default_base_url="https://work.dev.withpixie.dev/oauth/hydra",
        ),
        KnownApi(
            name=KnownApiName.PIXIE,
            # NOTE: We must use the hostname chosen for the Pixie cloud that is exposed via
            # the k8s ingress (rather than the internal service cloud-proxy-service.plc.svc),
            # because using the service does not work and results in HTTP-404s
            default_base_url="https://work.dev.withpixie.dev",
        ),
    )

    @staticmethod
    def factory_from_cli(
        api: KnownApi,
        args: argparse.Namespace,
    ) -> "ApiRequestHelper":
        """
        Factory method for instantiating a client from CLI arguments

        :param api: API to instantiate
        :param args: CLI args
        :return: API client
        """

        def arg_value(name: str) -> typing.Any:
            return vars(args)[api.get_option_name(name=name)]

        return ApiRequestHelper(
            api_base_path=typing.cast(str, arg_value(name="url")),
            verify=False
            if typing.cast(bool, arg_value(name="insecure"))
            else arg_value(name="verify"),
            debug_responses=args.verbose > 2,
            trace_json_responses=args.verbose > 2,
        )

    @classmethod
    def factory_by_known_api_name_from_cli(
        cls,
        api_name: KnownApiName,
        args: argparse.Namespace,
    ) -> "ApiRequestHelper":
        """
        Factory method for instantiating a client from CLI arguments for "known APIs"

        :param api_name: Name of the known API to instantiate
        :param args: CLI args
        :return: API client
        :raise Error: Unknown API
        """
        for api in cls.KNOWN_APIS:
            if api.name == api_name:
                return cls.factory_from_cli(api=api, args=args)
        raise Error(f"Unknown API {api_name}")

    @staticmethod
    def factory_from_env(
        api: KnownApi,
        env_variables_prefix: str = "",
    ) -> "ApiRequestHelper":
        """
        Factory method for instantiating a client from environment data

        :param api: API to instantiate
        :param env_variables_prefix: Prefix for the read environment variables
        :return: API client
        :raise Error: Unknown API or invalid/incomplete environment
        """

        def env_value(
            name: str,
            value_type: type,
            default_value: typing.Any = None,
            return_default_value: bool = False,
        ) -> typing.Any:
            full_option_name = (
                f"{env_variables_prefix}{api.get_option_name(name=name)}"
            ).upper()
            try:
                value = os.environ[full_option_name]
            except KeyError as err:
                if return_default_value:
                    return default_value
                raise Error(
                    f"Cannot instantiate {api.name.value} from env: missing {full_option_name}"
                ) from err
            if value_type == str:
                return value
            if value_type == bool:
                if value.lower() == "true":
                    return True
                if value.lower() == "false":
                    return False
                raise Error(
                    f"Cannot instantiate {api.name.value} from env: invalid bool value {value} "
                    f"for {full_option_name}"
                )
            assert False, f"Unsupported env-variable type {value_type.__name__}"

        debug_responses = env_value(
            name="DEBUG_RESPONSES",
            value_type=bool,
            default_value=False,
            return_default_value=True,
        )
        return ApiRequestHelper(
            api_base_path=env_value(name="url", value_type=str),
            verify=False
            if env_value(
                name="insecure",
                value_type=bool,
                default_value=False,
                return_default_value=True,
            )
            else env_value(name="verify", value_type=str, return_default_value=True),
            debug_responses=debug_responses,
            trace_json_responses=debug_responses,
        )

    @classmethod
    def factory_by_known_api_name_from_env(
        cls,
        api_name: KnownApiName,
        env_variables_prefix: str = "",
    ) -> "ApiRequestHelper":
        """
        Factory method for instantiating a client from CLI arguments for "known APIs"

        :param api_name: Name of the known API to instantiate
        :param env_variables_prefix: Prefix for the read environment variables
        :return: API client
        :raise Error: Unknown API or invalid/incomplete environment
        """
        for api in cls.KNOWN_APIS:
            if api.name == api_name:
                return cls.factory_from_env(
                    api=api, env_variables_prefix=env_variables_prefix
                )
        raise Error(f"Unknown API {api_name}")

    def __init__(
        self,
        api_base_path: str,
        verify: typing.Optional[typing.Union[str, bool]] = None,
        proxies: typing.Optional[dict] = None,
        debug_responses: bool = False,
        trace_json_responses: bool = False,
    ) -> None:
        """
        :param api_base_path: Prefix for any API request
        :param verify: Optional override to verify validity of SSL connections.
            See `requests.request()`
        :param proxies: Optional proxies to use for requests.
            See `requests.request()`
        :param debug_responses: If `True`, log detailed API responses.
            NOTE: This may leak credentials to logs; do not use in production environments where
            logs are captured
        :param trace_json_responses: If `True`, print detailed JSON API responses to console.
            Useful mostly for debugging.
            NOTE: This may leak credentials to logs; do not use in production environments where
            logs are captured
        """
        self._logger = logging.getLogger(self.__class__.__name__)
        self._api_base_path = api_base_path
        self._verify = verify
        self._proxies = proxies
        self._debug_responses = debug_responses
        self._trace_json_responses = trace_json_responses

    def request(
        self,
        method: str,
        path: str,
        raise_non_success_status: bool = True,
        ignored_error_status: typing.Optional[
            typing.Collection[http.HTTPStatus]
        ] = None,
        **kwargs,
    ) -> requests.Response:
        """
        Do a single API request

        :param method: Request method (e.g., GET, POST, DELETE, etc.)
        :param path: Path (appended to the `api_base_path`) to invoke
        :param raise_non_success_status: If `True`, raise an exception for any non-success
            response status-codes
        :param ignored_error_status: If `raise_non_success_status` is `True`, additionally ignore
            these non-success response status codes
        :param kwargs: Optional arguments. See `requests.request()`
        :raise ApiError: Invoking API failed
        :return: API response
        """
        url = self._api_base_path + path
        if self._verify is not None and "verify" not in kwargs:
            kwargs["verify"] = self._verify
        if self._proxies is not None and "proxies" not in kwargs:
            kwargs["proxies"] = self._proxies

        self._logger.debug("Invoking API request: %s", url)
        try:
            response = requests.request(method=method, url=url, **kwargs)
        except requests.RequestException as err:
            self._logger.warning("Invocation of %s failed: %s", url, err)
            raise ApiError(f"Invocation of {url} failed: {err}") from err

        if self._debug_responses:
            self._logger.debug("API request %s response: %s", url, response.text)
        if self._trace_json_responses:
            try:
                response_json = response.json()
            except ValueError:
                pass  # possibly not JSON
            else:
                pprint.pprint(response_json)

        if raise_non_success_status:
            try:
                response.raise_for_status()
            except requests.RequestException as err:
                if (
                    ignored_error_status is not None
                    and response.status_code in ignored_error_status
                ):
                    self._logger.debug(
                        "Ignoring accepted error response status of %s: %s",
                        url,
                        response.status_code,
                    )
                else:
                    self._logger.warning(
                        "Invocation of %s returned non-success (%s): %s",
                        url,
                        response.status_code,
                        err,
                    )
                    raise ApiError(
                        f"Invocation of {url} returned {response.status_code}"
                    ) from err

        return response

    def get(
        self,
        path: str,
        raise_non_success_status: bool = True,
        ignored_error_status: typing.Optional[
            typing.Collection[http.HTTPStatus]
        ] = None,
        **kwargs,
    ) -> requests.Response:
        """
        Do a single GET API request

        :param path: Path (appended to the `api_base_path`) to invoke
        :param raise_non_success_status: If `True`, raise an exception for any non-success
            response status-codes
        :param ignored_error_status: If `raise_non_success_status` is `True`, additionally ignore
            these non-success response status codes
        :param kwargs: Optional arguments. See `requests.request()`
        :raise ApiError: Invoking API failed
        :return: API response
        """
        return self.request(
            method="GET",
            path=path,
            raise_non_success_status=raise_non_success_status,
            ignored_error_status=ignored_error_status,
            **kwargs,
        )

    def post(
        self,
        path: str,
        raise_non_success_status: bool = True,
        ignored_error_status: typing.Optional[
            typing.Collection[http.HTTPStatus]
        ] = None,
        **kwargs,
    ) -> requests.Response:
        """
        Do a single POST API request

        :param path: Path (appended to the `api_base_path`) to invoke
        :param raise_non_success_status: If `True`, raise an exception for any non-success
            response status-codes
        :param ignored_error_status: If `raise_non_success_status` is `True`, additionally ignore
            these non-success response status codes
        :param kwargs: Optional arguments. See `requests.request()`
        :raise ApiError: Invoking API failed
        :return: API response
        """
        return self.request(
            method="POST",
            path=path,
            raise_non_success_status=raise_non_success_status,
            ignored_error_status=ignored_error_status,
            **kwargs,
        )

    def delete(
        self,
        path: str,
        raise_non_success_status: bool = True,
        ignored_error_status: typing.Optional[
            typing.Collection[http.HTTPStatus]
        ] = None,
        **kwargs,
    ) -> requests.Response:
        """
        Do a single DELETE API request

        :param path: Path (appended to the `api_base_path`) to invoke
        :param raise_non_success_status: If `True`, raise an exception for any non-success
            response status-codes
        :param ignored_error_status: If `raise_non_success_status` is `True`, additionally ignore
            these non-success response status codes
        :param kwargs: Optional arguments. See `requests.request()`
        :raise ApiError: Invoking API failed
        :return: API response
        """
        return self.request(
            method="DELETE",
            path=path,
            raise_non_success_status=raise_non_success_status,
            ignored_error_status=ignored_error_status,
            **kwargs,
        )


class IdentityAdminApiClient:
    """API client for administration of Pixie identities"""

    # OAuth scope for clients in Hydra. This must be the scope used by Pixie when authenticating
    # clients. Do not change from the default unless you know what you are doing
    PIXIE_AUTH_SCOPE = "vizier"

    @staticmethod
    def factory_from_env(
        env_variables_prefix: str = "",
    ) -> "IdentityAdminApiClient":
        """
        Factory for creating the API client from environment variables

        :param env_variables_prefix: Prefix for the read environment variables)
        :return: API client
        :raise Error: Invalid/incomplete environment
        """
        return IdentityAdminApiClient(
            kratos_api_client=ApiRequestHelper.factory_by_known_api_name_from_env(
                api_name=KnownApiName.KRATOS_ADMIN,
                env_variables_prefix=env_variables_prefix,
            ),
            hydra_api_client=ApiRequestHelper.factory_by_known_api_name_from_env(
                api_name=KnownApiName.HYDRA_ADMIN,
                env_variables_prefix=env_variables_prefix,
            ),
        )

    def __init__(
        self,
        kratos_api_client: ApiRequestHelper,
        hydra_api_client: ApiRequestHelper,
        kratos_schema_id: str = "default",
        grant_types: typing.Collection[str] = ("client_credentials",),
        auth_scope: str = PIXIE_AUTH_SCOPE,
    ) -> None:
        """
        :param kratos_api_client: Handler for issuing requests to the Kratos administration API
        :param hydra_api_client: Handler for issuing requests to the Hydra administration API
        :param kratos_schema_id: Name of the schema for validating the Kratos identity traits
        :param grant_types: OAuth grant-types to use. We use username+password auth and assume
            machine-to-machine communication of a trusted client, thus we use "client_credentials".
            See class doc-string for details
        :param auth_scope: OAuth scope to use when registering the client in Hydra. This must
            be the scope used by Pixie when authenticating clients. Do not change from the
            default unless you know what you are doing
        """
        self._logger = logging.getLogger(self.__class__.__name__)
        self._kratos_api_client = kratos_api_client
        self._hydra_api_client = hydra_api_client
        self._kratos_schema_id = kratos_schema_id
        self._grant_types = grant_types
        self._auth_scope = auth_scope

    def delete_identity(self, identity_id: str, ignore_missing: bool = False) -> bool:
        """
        Delete the identity

        NOTE: We use the same client ID between Kratos and Hydra. The ID is generated in Kratos,
        as it forces auto-generation. Hydra allows us to pass the ID in.

        :param identity_id: ID of the identity to delete
        :param ignore_missing: If `True`, ignore if deletion ends up with a "not found"
            error (e.g., in case of a race condition between listing and deleting)
        :return: `True` if the identity was found and deleted; `False` otherwise
        :raise ApiError: The identity was found but deletion failed
        """
        self._logger.info("Deleting identity: %s", identity_id)

        urlencoded_identity_id = urllib.parse.quote(identity_id)
        ignored_error_status = [http.HTTPStatus.NOT_FOUND] if ignore_missing else None

        self._logger.debug("Deleting Hydra client: %s", identity_id)
        response = self._hydra_api_client.delete(
            path=f"/clients/{urlencoded_identity_id}",
            ignored_error_status=ignored_error_status,
        )
        deleted_hydra_client = response.status_code != http.HTTPStatus.NOT_FOUND
        if deleted_hydra_client:
            self._logger.debug("Hydra client deletion successful: %s", identity_id)
        else:
            self._logger.warning(
                "Hydra client %s deletion failed: identity not found", identity_id
            )

        self._logger.debug("Deleting Kratos identity: %s", identity_id)
        self._kratos_api_client.delete(
            path=f"/admin/identities/{urlencoded_identity_id}",
            ignored_error_status=ignored_error_status,
        )
        deleted_kratos_identity = response.status_code != http.HTTPStatus.NOT_FOUND
        if deleted_kratos_identity:
            self._logger.debug("Kratos identity deletion successful: %s", identity_id)
        else:
            self._logger.warning(
                "Kratos identity %s deletion failed: identity not found",
                identity_id,
            )

        return deleted_hydra_client or deleted_kratos_identity

    def delete_identity_by_email_address(
        self,
        email_address: str,
        ignore_missing: bool = True,
    ) -> bool:
        """
        Find the identity with the given email address and delete it

        :param email_address: Email address associated with the identity to delete
        :param ignore_missing: If `True`, ignore if deletion ends up with a "not found"
            error (e.g., in case of a race condition between listing and deleting)
        :return: `True` if the identity was found and deleted; `False` otherwise
        :raise ApiError: The identity was found but deletion failed
        """
        self._logger.info("Finding identity for deletion: %s", email_address)
        response = self._kratos_api_client.get("/admin/identities")
        for entry in response.json():
            try:
                identity_id = entry["id"]
                identity_email = entry["traits"]["email"]
            except (KeyError, ValueError, TypeError) as err:
                self._logger.warning(
                    "Got invalid entity from Kratos API (invalid %s): %s", err, entry
                )
                continue
            if identity_email == email_address:
                self._logger.debug(
                    "Found identity to delete for email address %s: %s",
                    identity_email,
                    identity_id,
                )
                return self.delete_identity(
                    identity_id=identity_id,
                    ignore_missing=ignore_missing,
                )
        return False

    def _create_kratos_identity(self, email_address: str, password: str) -> str:
        """
        Create an identity in Kratos

        :param email_address: Email address for the identity
        :param password: Identity password
        :return: ID of the newly created Kratos identity
        :raise ApiError: Talking to Kratos API failed
        :raise Error: Cannot create identity
        """
        self._logger.debug("Creating identity in Kratos: %s", email_address)
        response = self._kratos_api_client.post(
            path="/admin/identities",
            json={
                "schema_id": self._kratos_schema_id,
                "traits": {
                    "email": email_address,
                },
                "credentials": {
                    "password": {
                        "config": {
                            "password": password,
                        },
                    },
                },
            },
            headers={"Accept": "application/json"},
            # we have special handling below (with better logging)
            ignored_error_status=[http.HTTPStatus.CONFLICT],
        )
        if response.status_code == http.HTTPStatus.CONFLICT:
            self._logger.warning("Cannot add Kratos identity %s: exists", email_address)
            raise Error("Kratos identity exists")
        if response.status_code != http.HTTPStatus.CREATED:
            self._logger.warning(
                "Unexpected Kratos API response for identity creation: %s", response
            )
            raise ApiError(
                f"Unexpected Kratos API response (HTTP-{response.status_code})"
            )
        response_json = response.json()
        identity_id = response_json.get("id")
        if not identity_id:
            self._logger.warning(
                "Unexpected Kratos API response data in identity creation: %s",
                response_json,
            )
            raise ApiError("Unexpected Kratos API response (missing ID)")
        return identity_id

    def create_identity(
        self,
        client_secret: str,
        email_address: str,
        password: str,
    ) -> str:
        """
        Create an identity

        :param client_secret: Client secret for authenticating the Machine-To-Machine OAuth client
        :param email_address: Email address for the identity
        :param password: Identity password
        :return: ID of the newly created identity
        :raise ApiError: Talking to identity APIs failed
        :raise Error: Cannot create identity
        """
        identity_id = self._create_kratos_identity(
            email_address=email_address,
            password=password,
        )
        # Now that we have an identity in Kratos, we create an OAuth client using the same ID (so
        # we don't need to maintain multiple IDs)
        try:
            self._hydra_api_client.post(
                path="/clients",
                json={
                    "client_id": identity_id,
                    "client_name": f"Machine-to-machine OAuth client for identity {identity_id}, "
                    f"administrative identity of Pixie Org registered for {email_address}",
                    "client_secret": client_secret,
                    "grant_types": list(self._grant_types),
                    "scope": self._auth_scope,
                },
            )
        except Error as err:
            self._logger.warning(
                "Creating client %s for %s in Hydra failed: %s",
                identity_id,
                email_address,
                err,
            )
            raise
        return identity_id


class OAuthApiClient:
    """
    API client for performing authentication

    NOTE: In terms of OAuth, this script is client and resource owner at once. As such, we can
    use a simplified Machine-To-Machine auth-flow that does not involve user-consent via a browser,
    as that wouldn't make sense here. For details, see

    https://auth0.com/docs/get-started/authentication-and-authorization-flow/client-credentials-flow

    This Machine-To-Machine auth-flow is what is implemented in this class.
    """

    @staticmethod
    def factory_from_env(
        env_variables_prefix: str = "",
    ) -> "OAuthApiClient":
        """
        Factory for creating the API client from environment variables

        :param env_variables_prefix: Prefix for the read environment variables)
        :return: API client
        :raise Error: Invalid/incomplete environment
        """
        return OAuthApiClient(
            api_client=ApiRequestHelper.factory_by_known_api_name_from_env(
                api_name=KnownApiName.HYDRA_USER,
                env_variables_prefix=env_variables_prefix,
            ),
        )

    def __init__(self, api_client: ApiRequestHelper) -> None:
        """
        :param api_client: Handler for issuing requests to the Hydra Auth ("user") API
        """
        self._logger = logging.getLogger(self.__class__.__name__)
        self._api_client = api_client

    def get_access_token(
        self,
        username: str,
        password: str,
        auth_scope: str = IdentityAdminApiClient.PIXIE_AUTH_SCOPE,
    ) -> str:
        """
        Perform a login to get an access token

        :param username: Identity to login
        :param password: Password to use for authentication
        :param auth_scope: OAuth scope to request. See the corresponding parameter in
            `IdentityAdminApiClient` for details
        :raise ApiError: The identity was found but deletion failed
        """
        self._logger.debug("Performing password login: %s", username)
        response = self._api_client.post(
            path="/oauth2/token",
            auth=(username, password),
            data={
                "grant_type": "client_credentials",
                "scope": auth_scope,
            },
            headers={"Accept": "application/json"},
        )
        try:
            return response.json()["access_token"]
        except (KeyError, ValueError, TypeError) as err:
            self._logger.warning("Hydra oauth2 login did not return an access_token")
            raise ApiError("Invalid Hydra response") from err


class PixieRegistrationApiClient:
    """
    API client for performing Pixie registration

    NOTE: This API client is specifically for the Org registration and initial API access setup.
    Once this API's workflow has been completed, we should use the standard Pixie API for any
    complex workflows (i.e., think twice before extending this API client).
    """

    PIXIE_ORG_NAME_REGEXP = re.compile("^[a-zA-Z0-9][a-zA-Z0-9_.+-]{5,49}$")

    @staticmethod
    def factory_from_env(
        env_variables_prefix: str = "",
    ) -> "PixieRegistrationApiClient":
        """
        Factory for creating the API client from environment variables

        :param env_variables_prefix: Prefix for the read environment variables)
        :return: API client
        :raise Error: Invalid/incomplete environment
        """
        return PixieRegistrationApiClient(
            api_client=ApiRequestHelper.factory_by_known_api_name_from_env(
                api_name=KnownApiName.PIXIE,
                env_variables_prefix=env_variables_prefix,
            ),
        )

    def __init__(self, api_client: ApiRequestHelper) -> None:
        """
        :param api_client: Handler for issuing requests to the Pixie API
        """
        self._logger = logging.getLogger(self.__class__.__name__)
        self._api_client = api_client
        # NOTE: We don't use sessions but rather drag around the access token across all requests
        self._api_token = None
        # User and Pixie Org metadata provided on login (needed for various API calls)
        self._pixie_user_id = None
        self._pixie_user_email_address = None
        self._pixie_org_id = None
        self._pixie_org_name = None

    def _api_post(
        self,
        path: str,
        **kwargs,
    ) -> requests.Response:
        """
        Helper to send POST request to Pixie API

        NOTE: Can only be used once `self.login()` has been called successfully.

        :param path: Path to invoke
        :param kwargs: Optional arguments. See `requests.request()`
        :return: API response
        """
        if self._api_token is None:
            self._logger.warning("Missing login")
            raise Error("Missing Pixie API login")

        self._logger.debug("Performing API request: %s", path)
        return self._api_client.post(
            path=path, headers={"authorization": f"bearer {self._api_token}"}, **kwargs
        )

    def signup(
        self,
        org_name: str,
        access_token: str,
    ) -> None:
        """
        Perform (one-time) signup for creating the Pixie Org

        NOTE: This method does NOT perform a login. A subsequent call to `login()` is needed, even
        if this was previously done, as the Pixie API invalidates sessions on Org creation.

        :param org_name: Name of the Pixie Org to use
        :param access_token: Access-token to use for authentication
        :raise ApiError: Invoking Pixie API failed
        """
        self._logger.info("Performing signup for Pixie Org %s", org_name)

        if not self.PIXIE_ORG_NAME_REGEXP.match(org_name):
            self._logger.warning("Rejecting invalid Pixie Org: %s", org_name)
            raise Error("Invalid Pixie Org name")

        response = self._api_client.post(
            path="/api/auth/signup",
            json={
                "accessToken": access_token,
            },
            # we want to handle the error ourselves (see below)
            ignored_error_status=[http.HTTPStatus.INTERNAL_SERVER_ERROR],
        )
        if response.status_code == http.HTTPStatus.INTERNAL_SERVER_ERROR:
            # The Pixie API doesn't have a very good error-handling for this case: we need to
            # parse the response in a slightly brittle way
            if (
                response.text.strip()
                == "Failed to signup: cannot create duplicate user"
            ):
                self._logger.info("Pixie signup failed: Pixie Org exists")
                raise Error("Pixie Org signup failed: Org exists")

            self._logger.warning(
                "Pixie signup failed (status-code %s): %s",
                response.text,
                response.status_code,
            )
            raise Error("Pixie Org signup failed")

        try:
            pixie_token = response.json()["token"]
        except (KeyError, ValueError, TypeError) as err:
            self._logger.warning(
                "Pixie signup successful but missing token in response"
            )
            raise ApiError("Pixie Org signup did not return token") from err
        self._logger.info("Signup for Pixie Org %s successful", org_name)

        # temporarily sign in for the rest of the API calls - reset when we exit the function
        self._api_token = pixie_token
        try:
            # check auth working as expected
            self._api_post(path="/api/authorized")
            self._do_graphql_operation(
                operation_name="CreateOrgFromSetupOrgPage",
                variables={
                    "orgName": org_name,
                },
                query="""mutation CreateOrgFromSetupOrgPage($orgName: String!) {
                    CreateOrg(orgName: $orgName)
                }
                """,
            )
        finally:
            # seems Org creation messes with tokens. Enforce a login after signup, no matter what
            self._api_token = None

    def login(self, access_token: str) -> None:
        """
        Login to the Pixie API

        NOTE: Calling this method resets the connection to the API (if one exists), regardless of
        the outcome of the login.

        :param access_token: Access-token to use for authentication. If `None` is provided, use the
            access-token of a previous call to `login()`
        :raise ApiError: Invoking Pixie API failed
        """
        self._logger.debug("Performing API login")

        # see doc-string - we don't want a complicated/confusing state depending on the type of
        # failure
        self._api_token = None
        self._pixie_user_id = None
        self._pixie_user_email_address = None
        self._pixie_org_id = None
        self._pixie_org_name = None

        response = self._api_client.post(
            path="/api/auth/login",
            json={
                "accessToken": access_token,
            },
        )
        response_json = response.json()
        try:
            # NOTE: We update the client object (below) only if all values were read successfully,
            # hence the temporary variables
            pixie_token = response_json["token"]
            pixie_user_info = response_json["userInfo"]
            pixie_user_id = pixie_user_info["userID"]
            pixie_user_email_address = pixie_user_info["email"]
            pixie_org_info = response_json["orgInfo"]
            pixie_org_id = pixie_org_info["orgID"]
            pixie_org_name = pixie_org_info["orgName"]
        except (KeyError, ValueError, TypeError) as err:
            self._logger.warning(
                "Login successful but missing data in response: %s", err
            )
            raise ApiError("Pixie login did not return expected data") from err
        self._api_token = pixie_token
        self._pixie_user_id = pixie_user_id
        self._pixie_user_email_address = pixie_user_email_address
        self._pixie_org_id = pixie_org_id
        self._pixie_org_name = pixie_org_name

    def _do_graphql_operation(
        self,
        operation_name: str,
        query: str,
        variables: typing.Optional[dict] = None,
    ) -> dict:
        """
        Perform a graphql API request

        :param operation_name: Name of the operation to perform
        :param query: Operation to execute
        :return: Operation response
        :raise ApiError: Invoking Pixie API failed
        """
        self._logger.debug("Invoking GraphQL API method: %s", operation_name)
        response = self._api_post(
            path="/api/graphql",
            json={
                "operationName": operation_name,
                "variables": {} if variables is None else variables,
                "query": query,
            },
        )
        try:
            return response.json()["data"]
        except (KeyError, ValueError, TypeError) as err:
            self._logger.warning(
                "Invoking API method %s failed: missing %s", operation_name, err
            )
            raise ApiError(f"GraphQL API response invalid: {err}") from err

    def _create_id_entity(
        self,
        entity_type_name: str,
        create_operation_name: str,
        create_query_name: str,
        get_operation_name: str,
        get_query_name: str,
    ) -> typing.Tuple[str, str]:
        """
        Helper for creating ID:KEY entities in the API

        NOTE: Several entities in the API follow the same concepts/structures. This helper exists
        merely to avoid code duplication

        NOTE: The graphQL schema supports a "desc" field for most entities, but the API does not
        currently allow setting it in the mutations (it is not accepted as parameter).

        :param entity_type_name: Name of the API entity type to create (for logging)
        :param create_operation_name: Name of the operation to invoke for creation
        :param create_query_name: Name of the query to invoke for creation
        :param get_operation_name: Name of the operation to invoke for retrieval
        :param get_query_name: Query to invoke for retrieval
        :return: Entity ID and key
        :raise ApiError: Invoking Pixie API failed
        """
        self._logger.info("Creating %s", entity_type_name)

        data = self._do_graphql_operation(
            operation_name=create_operation_name,
            query=f"""mutation {create_operation_name}() {{
                {create_query_name}() {{
                    id
                    desc
                    createdAtMs
                    __typename
                }}
            }}
            """,
        )
        try:
            entity_id = data[create_query_name]["id"]
        except (KeyError, ValueError, TypeError) as err:
            self._logger.warning(
                "Invalid response from %s API: %s", create_operation_name, err
            )
            raise ApiError(
                f"Response from {create_operation_name} is invalid: {err}"
            ) from err

        self._logger.debug("Creating %s successful: %s", entity_type_name, entity_id)
        data = self._do_graphql_operation(
            operation_name=get_operation_name,
            variables={
                "id": entity_id,
            },
            query=f"""query {get_operation_name}($id: ID!) {{
                {get_query_name}(id: $id) {{
                    id
                    key
                    desc
                    __typename
                }}
            }}
            """,
        )
        try:
            entity_key = data[get_query_name]["key"]
        except (KeyError, ValueError, TypeError) as err:
            self._logger.warning(
                "Invalid response from %s API: %s", get_operation_name, err
            )
            raise ApiError(
                f"Response from {get_operation_name} invalid: {err}"
            ) from err

        self._logger.debug("Fetched %s %s secret", entity_type_name, entity_id)
        return entity_id, entity_key

    def create_api_key(self) -> typing.Tuple[str, str]:
        """
        Create an API key

        NOTE: The graphQL schema supports a "desc" field for API-keys, but the API does
        not currently allow setting it in the mutations (it is not accepted as parameter).

        :return: API key ID and key-secret
        :raise ApiError: Invoking Pixie API failed
        """
        return self._create_id_entity(
            entity_type_name="API key",
            create_operation_name="CreateAPIKeyFromAdminPage",
            create_query_name="CreateAPIKey",
            get_operation_name="getAPIKey",
            get_query_name="apiKey",
        )

    def create_deployment_key(self) -> typing.Tuple[str, str]:
        """
        Create a deployment-key

        NOTE: The graphQL schema supports a "desc" field for deployment-keys, but the API does
        not currently allow setting it in the mutations (it is not accepted as parameter).

        :return: Deployment key ID and key-secret
        :raise ApiError: Invoking Pixie API failed
        """
        return self._create_id_entity(
            entity_type_name="deployment key",
            create_operation_name="CreateDeploymentKeyFromAdminPage",
            create_query_name="CreateDeploymentKey",
            get_operation_name="getDeploymentKey",
            get_query_name="deploymentKey",
        )

    def configure_org_settings(
        self,
        enable_user_approvals: bool,
    ) -> None:
        """
        Configure the Pixie Org settings

        :param enable_user_approvals: If `True`, require new users to be approved before being
            able to access the org; Otherwise, Pixie will allow any user who registers in the org
            to log in and use Pixie without approval
        :raise ApiError: Invoking Pixie API failed
        """
        self._logger.info("Configuring Pixie user approvals: %s", enable_user_approvals)

        # NOTE: Once we have multiple options supported, we might want to allow passing in `None`
        # to update only those parameters that are `True` or `False`. Alternatively, we can get
        # the current settings, patch, and post back
        self._do_graphql_operation(
            operation_name="UpdateOrgApprovalSetting",
            variables={
                "orgID": self._pixie_org_id,
                "enableApprovals": enable_user_approvals,
            },
            query="""mutation UpdateOrgApprovalSetting($orgID: ID!, $enableApprovals: Boolean!) {
                UpdateOrgSettings(
                    orgID: $orgID
                    orgSettings: {enableApprovals: $enableApprovals}
                ) {
                    id
                    name
                    enableApprovals
                    __typename
                }
            }
            """,
        )
        self._logger.info("Pixie Org approval setting: updated successfully")

    def configure_user_settings(
        self,
        enable_analytics: bool,
    ) -> None:
        """
        Configure the Pixie Org settings

        :param enable_analytics: If `True`, enable tracking of UI analytics for this
            user; Otherwise disable tracking
        :raise ApiError: Invoking Pixie API failed
        """
        self._logger.info(
            "Configuring Pixie user-settings: ui-analytics=%s", enable_analytics
        )

        # NOTE: Once we have multiple options supported, we might want to allow passing in `None`
        # to update only those parameters that are `True` or `False`. Alternatively, we can get
        # the current settings, patch, and post back
        self._do_graphql_operation(
            operation_name="UpdateUserAnalyticOptOut",
            variables={
                "analyticsOptout": not enable_analytics,
            },
            query="""mutation UpdateUserAnalyticOptOut($analyticsOptout: Boolean!) {
                UpdateUserSettings(
                    settings: {analyticsOptout: $analyticsOptout}
                ) {
                    id
                    analyticsOptout
                    __typename
                }
            }
            """,
        )
        self._logger.info("Pixie Org user settings: updated successfully")
