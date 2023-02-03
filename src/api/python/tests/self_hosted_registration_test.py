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

import argparse
import http
import os
import unittest
import unittest.mock

import ddt
import requests

from self_hosted_registration import registration


# Some of the test methods don't have doc-strings, because we use `ddt` which will add
# doc-strings based on the values that are added from `ddt.data`. If we had a doc-string,
# the decorator would not do that and it's hard to track down which tests fail
#
# And we want to keep tests together - breaking it up artificially doesn't make sense
# for tests
#
# pylint: disable=missing-function-docstring,too-many-lines


@ddt.ddt
class TestApiRequestHelper(unittest.TestCase):
    """
    Test request helpers in `ApiRequestHelper`
    """

    def test_factory_from_cli(self) -> None:
        """
        Test parsing API configs from `argparse`
        """
        args = argparse.Namespace()
        args.kratos_admin_url = "test-kratos-url"
        args.kratos_admin_insecure = True
        args.verbose = True

        # get a reference before mocking, so we can call the original function we want to test
        test_factory = registration.ApiRequestHelper.factory_by_known_api_name_from_cli

        mock_helper = unittest.mock.MagicMock(registration.ApiRequestHelper)
        with unittest.mock.patch(
            "self_hosted_registration.registration.ApiRequestHelper", autospec=True
        ) as mock_constructor:
            mock_constructor.return_value = mock_helper
            helper = test_factory(
                api_name=registration.KnownApiName.KRATOS_ADMIN,
                args=args,
            )
            mock_constructor.assert_called_once_with(
                api_base_path="test-kratos-url",
                debug_responses=False,
                trace_json_responses=False,
                verify=False,
            )
        self.assertIs(helper, mock_helper)

    def test_factory_from_cli_unknown_api(self) -> None:
        """
        Test parsing API configs from `argparse` of an unknown API

        NOTE: This test is a bit constructed and mostly for code-coverage. As we're using
        enums, the covered path is usually not reachable but implemented for completeness sake
        """
        with self.assertRaises(registration.Error) as err:
            registration.ApiRequestHelper.factory_by_known_api_name_from_cli(
                api_name=unittest.mock.MagicMock(registration.KnownApiName),
                args=argparse.Namespace(),
            )
        self.assertIn(
            "Unknown API <MagicMock spec='KnownApiName' id=", str(err.exception)
        )

    @unittest.mock.patch.dict(
        os.environ,
        {
            "TEST_KRATOS_ADMIN_URL": "test-kratos-url",
            "TEST_KRATOS_ADMIN_DEBUG_RESPONSES": "true",
            "TEST_KRATOS_ADMIN_VERIFY": "test-verify",
        },
        clear=True,
    )
    def test_factory_from_env(self) -> None:
        """
        Test parsing API configs from environment
        """
        # get a reference before mocking, so we can call the original function we want to test
        test_factory = registration.ApiRequestHelper.factory_by_known_api_name_from_env

        mock_helper = unittest.mock.MagicMock(registration.ApiRequestHelper)
        with unittest.mock.patch(
            "self_hosted_registration.registration.ApiRequestHelper", autospec=True
        ) as mock_constructor:
            mock_constructor.return_value = mock_helper
            helper = test_factory(
                api_name=registration.KnownApiName.KRATOS_ADMIN,
                env_variables_prefix="TEST_",
            )
            mock_constructor.assert_called_once_with(
                api_base_path="test-kratos-url",
                debug_responses=True,
                trace_json_responses=True,
                verify="test-verify",
            )
        self.assertIs(helper, mock_helper)

    @unittest.mock.patch.dict(
        os.environ,
        {
            "TEST_KRATOS_ADMIN_URL": "test-kratos-url",
        },
        clear=True,
    )
    def test_factory_from_env_minimal_env(self) -> None:
        """
        Test parsing API configs from environment: minimal environment settings
        """
        # get a reference before mocking, so we can call the original function we want to test
        test_factory = registration.ApiRequestHelper.factory_by_known_api_name_from_env

        mock_helper = unittest.mock.MagicMock(registration.ApiRequestHelper)
        with unittest.mock.patch(
            "self_hosted_registration.registration.ApiRequestHelper", autospec=True
        ) as mock_constructor:
            mock_constructor.return_value = mock_helper
            helper = test_factory(
                api_name=registration.KnownApiName.KRATOS_ADMIN,
                env_variables_prefix="TEST_",
            )
            mock_constructor.assert_called_once_with(
                api_base_path="test-kratos-url",
                debug_responses=False,
                trace_json_responses=False,
                verify=None,
            )
        self.assertIs(helper, mock_helper)

    @unittest.mock.patch.dict(
        os.environ,
        {
            "TEST_KRATOS_ADMIN_URL": "test-kratos-url",
            "TEST_KRATOS_ADMIN_INSECURE": "true",
        },
        clear=True,
    )
    def test_factory_from_env_insecure(self) -> None:
        """
        Test parsing API configs from environment: disable SSL validation
        """
        # get a reference before mocking, so we can call the original function we want to test
        test_factory = registration.ApiRequestHelper.factory_by_known_api_name_from_env

        mock_helper = unittest.mock.MagicMock(registration.ApiRequestHelper)
        with unittest.mock.patch(
            "self_hosted_registration.registration.ApiRequestHelper", autospec=True
        ) as mock_constructor:
            mock_constructor.return_value = mock_helper
            helper = test_factory(
                api_name=registration.KnownApiName.KRATOS_ADMIN,
                env_variables_prefix="TEST_",
            )
            mock_constructor.assert_called_once_with(
                api_base_path="test-kratos-url",
                debug_responses=False,
                trace_json_responses=False,
                verify=False,
            )
        self.assertIs(helper, mock_helper)

    @unittest.mock.patch.dict(
        os.environ,
        {
            "TEST_KRATOS_ADMIN_URL": "test-kratos-url",
            "TEST_KRATOS_ADMIN_INSECURE": "false",
        },
        clear=True,
    )
    def test_factory_from_env_secure_explicit(self) -> None:
        """
        Test parsing API configs from environment: configure SSL validation

        NOTE: This is covering extra code-paths that are actually identical to the default
        configuration
        """
        # get a reference before mocking, so we can call the original function we want to test
        test_factory = registration.ApiRequestHelper.factory_by_known_api_name_from_env

        mock_helper = unittest.mock.MagicMock(registration.ApiRequestHelper)
        with unittest.mock.patch(
            "self_hosted_registration.registration.ApiRequestHelper", autospec=True
        ) as mock_constructor:
            mock_constructor.return_value = mock_helper
            helper = test_factory(
                api_name=registration.KnownApiName.KRATOS_ADMIN,
                env_variables_prefix="TEST_",
            )
            mock_constructor.assert_called_once_with(
                api_base_path="test-kratos-url",
                debug_responses=False,
                trace_json_responses=False,
                verify=None,
            )
        self.assertIs(helper, mock_helper)

    @unittest.mock.patch.dict(
        os.environ,
        {
            "TEST_KRATOS_ADMIN_URL": "test-kratos-url",
            "TEST_KRATOS_ADMIN_INSECURE": "this-is-not-a-bool",
        },
        clear=True,
    )
    def test_factory_from_env_invalid_config(self) -> None:
        """
        Test parsing API configs from environment: invalid configuration
        """
        # get a reference before mocking, so we can call the original function we want to test
        test_factory = registration.ApiRequestHelper.factory_by_known_api_name_from_env

        with unittest.mock.patch(
            "self_hosted_registration.registration.ApiRequestHelper", autospec=True
        ) as mock_constructor:
            with self.assertRaises(registration.Error) as err:
                test_factory(
                    api_name=registration.KnownApiName.KRATOS_ADMIN,
                    env_variables_prefix="TEST_",
                )
            mock_constructor.assert_not_called()
        self.assertEqual(
            str(err.exception),
            "Cannot instantiate Kratos Admin from env: invalid bool value this-is-not-a-bool "
            "for TEST_KRATOS_ADMIN_INSECURE",
        )

    @unittest.mock.patch.dict(os.environ, {}, clear=True)
    def test_factory_from_env_incomplete_env(self) -> None:
        """
        Test parsing API configs from environment: missing values
        """
        # get a reference before mocking, so we can call the original function we want to test
        test_factory = registration.ApiRequestHelper.factory_by_known_api_name_from_env

        with unittest.mock.patch(
            "self_hosted_registration.registration.ApiRequestHelper", autospec=True
        ) as mock_constructor:
            with self.assertRaises(registration.Error) as err:
                test_factory(
                    api_name=registration.KnownApiName.KRATOS_ADMIN,
                    env_variables_prefix="TEST_",
                )
            mock_constructor.assert_not_called()
        self.assertEqual(
            str(err.exception),
            "Cannot instantiate Kratos Admin from env: missing TEST_KRATOS_ADMIN_URL",
        )

    def test_factory_from_env_unknown_api(self) -> None:
        """
        Test parsing API configs from environment of an unknown API

        NOTE: This test is a bit constructed and mostly for code-coverage. As we're using
        enums, the covered path is usually not reachable but implemented for completeness sake
        """
        with self.assertRaises(registration.Error) as err:
            registration.ApiRequestHelper.factory_by_known_api_name_from_env(
                api_name=unittest.mock.MagicMock(registration.KnownApiName),
            )
        self.assertIn(
            "Unknown API <MagicMock spec='KnownApiName' id=", str(err.exception)
        )

    @unittest.mock.patch("requests.request", autospec=True)
    def test_request(self, mocked_request) -> None:
        """
        Test wrapper is propagating all parameters as expected to the requests library
        """
        mock_response = unittest.mock.MagicMock(requests.Response)
        mocked_request.return_value = mock_response

        test_helper = registration.ApiRequestHelper(
            api_base_path="http://test-base-path",
            verify=False,
            proxies={"test": "proxy"},
            debug_responses=True,
            trace_json_responses=True,
        )
        test_helper.request(
            method="PATCH",
            path="/test/path",
            ignored_error_status=[http.HTTPStatus.ACCEPTED],
            some_other_requests_parameter="test-parameter-value",
        )
        mocked_request.assert_called_once_with(
            method="PATCH",
            url="http://test-base-path/test/path",
            verify=False,
            proxies={"test": "proxy"},
            some_other_requests_parameter="test-parameter-value",
        )
        mock_response.raise_for_status.assert_called_once()

    @unittest.mock.patch("requests.request", autospec=True)
    def test_request_error(self, mocked_request) -> None:
        """
        Test wrapper is propagating request errors: failing request
        """
        mocked_request.side_effect = requests.RequestException("test-error")

        test_helper = registration.ApiRequestHelper(
            api_base_path="http://test-base-path"
        )
        with self.assertRaises(registration.ApiError) as err:
            test_helper.request(
                method="GET",
                path="",
            )
        self.assertEqual(
            str(err.exception),
            "Invocation of http://test-base-path failed: test-error",
        )
        mocked_request.assert_called_once_with(
            method="GET",
            url="http://test-base-path",
        )

    @unittest.mock.patch("requests.request", autospec=True)
    def test_request_response_error(self, mocked_request) -> None:
        """
        Test wrapper is propagating response errors: HTTP response is an error
        """
        mock_response = unittest.mock.MagicMock(requests.Response)
        mock_response.raise_for_status.side_effect = requests.RequestException(
            "test-error"
        )
        mock_response.json.side_effect = ValueError("test-error")
        mock_response.status_code = http.HTTPStatus.NOT_FOUND.value

        mocked_request.return_value = mock_response

        test_helper = registration.ApiRequestHelper(
            api_base_path="http://test-base-path",
            debug_responses=True,
            trace_json_responses=True,
        )
        with self.assertRaises(registration.ApiError) as err:
            test_helper.request(
                method="GET",
                path="",
            )
        self.assertEqual(
            str(err.exception),
            "Invocation of http://test-base-path returned 404",
        )
        mocked_request.assert_called_once_with(
            method="GET",
            url="http://test-base-path",
        )
        mock_response.json.assert_called_once()
        mock_response.raise_for_status.assert_called_once()

    @unittest.mock.patch("requests.request", autospec=True)
    def test_request_ignore_response_error(self, mocked_request) -> None:
        """
        Test wrapper is propagating response errors: HTTP response is an ignored error
        """
        mock_response = unittest.mock.MagicMock(requests.Response)
        mock_response.raise_for_status.side_effect = requests.RequestException(
            "test-error"
        )
        mock_response.status_code = http.HTTPStatus.NOT_FOUND.value

        mocked_request.return_value = mock_response

        test_helper = registration.ApiRequestHelper(
            api_base_path="http://test-base-path",
            debug_responses=True,
            trace_json_responses=True,
        )
        response = test_helper.request(
            method="GET",
            path="",
            ignored_error_status=[http.HTTPStatus.NOT_FOUND],
        )
        self.assertEqual(response.status_code, http.HTTPStatus.NOT_FOUND)
        mocked_request.assert_called_once_with(
            method="GET",
            url="http://test-base-path",
        )
        mock_response.raise_for_status.assert_called_once()

    @ddt.data("GET", "POST", "DELETE")
    def test_request_for_request_method(self, request_method: str) -> None:
        # Test <request_method> requests propagate parameters as expected
        #
        # NOTE: Most functionality is tested in `self.test_request*`. This test merely ensures
        # this internal helper is invoked properly as the tested method does not add logic itself
        test_helper = registration.ApiRequestHelper(
            api_base_path="http://test-base-path",
        )
        mock_response = unittest.mock.MagicMock(requests.Response)

        with unittest.mock.patch.object(
            registration.ApiRequestHelper, "request"
        ) as mock_request:
            mock_request.return_value = mock_response

            test_helper_method = getattr(test_helper, request_method.lower())
            response = test_helper_method(
                path="/test/path",
                raise_non_success_status=False,
                ignored_error_status=[http.HTTPStatus.NOT_FOUND],
                some_other_requests_parameter="test-parameter-value",
            )

            self.assertIs(response, mock_response)
            mock_request.assert_called_once_with(
                method=request_method,
                path="/test/path",
                raise_non_success_status=False,
                ignored_error_status=[http.HTTPStatus.NOT_FOUND],
                some_other_requests_parameter="test-parameter-value",
            )

    def test_post(self) -> None:
        """
        Test POST requests propagate parameters as expected

        NOTE: Most functionality is tested in `self.test_request*`. This test merely ensures this
        internal helper is invoked properly as the tested method does not add logic itself
        """
        test_helper = registration.ApiRequestHelper(
            api_base_path="http://test-base-path",
        )
        mock_response = unittest.mock.MagicMock(requests.Response)

        with unittest.mock.patch.object(
            registration.ApiRequestHelper, "request"
        ) as mock_request:
            mock_request.return_value = mock_response
            response = test_helper.post(
                path="/test/path",
                raise_non_success_status=False,
                ignored_error_status=[http.HTTPStatus.NOT_FOUND],
                some_other_requests_parameter="test-parameter-value",
            )
            self.assertIs(response, mock_response)
            mock_request.assert_called_once_with(
                method="POST",
                path="/test/path",
                raise_non_success_status=False,
                ignored_error_status=[http.HTTPStatus.NOT_FOUND],
                some_other_requests_parameter="test-parameter-value",
            )


class TestIdentityAdminApiClient(unittest.TestCase):
    """
    Test `IdentityAdminApiClient`
    """

    @unittest.mock.patch.object(
        # NOTE: We use `unittest.mock.patch.object()` because a the mock module has a bug that
        # does not allow us to patch a staticmethod (and also doesn't allow `autospec=True`)
        #
        # https://bugs.python.org/issue23078
        #
        registration.ApiRequestHelper,
        "factory_by_known_api_name_from_env",
    )
    def test_factory_from_env(self, mock_api_request_helper_factory) -> None:
        """
        Test instantiating from the environment
        """
        # get a reference before mocking, so we can call the original function we want to test
        test_factory = registration.IdentityAdminApiClient.factory_from_env

        mock_kratos_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_identity_client = unittest.mock.MagicMock(
            registration.IdentityAdminApiClient
        )
        with unittest.mock.patch(
            "self_hosted_registration.registration.IdentityAdminApiClient",
            autospec=True,
        ) as mock_constructor:
            mock_api_request_helper_factory.side_effect = [
                mock_kratos_api_client,
                mock_hydra_api_client,
            ]
            mock_constructor.return_value = mock_identity_client

            identity_client = test_factory(env_variables_prefix="TEST_")
            self.assertIs(identity_client, mock_identity_client)

            mock_constructor.assert_called_once_with(
                kratos_api_client=mock_kratos_api_client,
                hydra_api_client=mock_hydra_api_client,
            )

        mock_api_request_helper_factory.assert_has_calls(
            [
                unittest.mock.call(
                    api_name=registration.KnownApiName.KRATOS_ADMIN,
                    env_variables_prefix="TEST_",
                ),
                unittest.mock.call(
                    api_name=registration.KnownApiName.HYDRA_ADMIN,
                    env_variables_prefix="TEST_",
                ),
            ]
        )

    def test_delete_identity(self) -> None:
        """
        Test deleting an identity
        """
        mock_kratos_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)

        test_identity_client = registration.IdentityAdminApiClient(
            kratos_api_client=mock_kratos_api_client,
            hydra_api_client=mock_hydra_api_client,
        )
        deleted = test_identity_client.delete_identity(identity_id="test-identity-id")
        self.assertTrue(deleted)

        mock_kratos_api_client.delete.assert_called_once_with(
            path="/admin/identities/test-identity-id",
            ignored_error_status=None,
        )
        mock_hydra_api_client.delete.assert_called_once_with(
            path="/clients/test-identity-id",
            ignored_error_status=None,
        )

    def test_delete_identity_unknown_identity(self) -> None:
        """
        Test deleting an identity that does not exist
        """
        mock_kratos_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_kratos_response = unittest.mock.MagicMock(requests.Response)
        mock_kratos_response.status_code = http.HTTPStatus.NOT_FOUND.value
        mock_kratos_api_client.delete.return_value = mock_kratos_response

        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_hydra_response = unittest.mock.MagicMock(requests.Response)
        mock_hydra_response.status_code = http.HTTPStatus.NOT_FOUND.value
        mock_hydra_api_client.delete.return_value = mock_hydra_response

        test_identity_client = registration.IdentityAdminApiClient(
            kratos_api_client=mock_kratos_api_client,
            hydra_api_client=mock_hydra_api_client,
        )
        deleted = test_identity_client.delete_identity(
            identity_id="test-identity-id",
            ignore_missing=True,
        )
        self.assertFalse(deleted)

        mock_kratos_api_client.delete.assert_called_once_with(
            path="/admin/identities/test-identity-id",
            ignored_error_status=[http.HTTPStatus.NOT_FOUND],
        )
        mock_hydra_api_client.delete.assert_called_once_with(
            path="/clients/test-identity-id",
            ignored_error_status=[http.HTTPStatus.NOT_FOUND],
        )

    def test_delete_identity_by_email_address(self) -> None:
        """
        Test deleting an identity from its email address
        """
        mock_kratos_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_kratos_response = unittest.mock.MagicMock(requests.Response)
        mock_kratos_response.status_code = http.HTTPStatus.NOT_FOUND.value
        mock_kratos_response.json.return_value = [
            {
                "id": "some-ignored-id",
                "traits": {
                    "email": "some-ignored@test",
                },
            },
            {"id": "some-corrupted-entry-that-should-be-ignored-too"},
            {
                "id": "test-identity-id",
                "traits": {
                    "email": "test-identity@test",
                },
            },
        ]
        mock_kratos_api_client.get.return_value = mock_kratos_response

        test_identity_client = registration.IdentityAdminApiClient(
            kratos_api_client=mock_kratos_api_client,
            hydra_api_client=unittest.mock.MagicMock(registration.ApiRequestHelper),
        )

        with unittest.mock.patch.object(
            registration.IdentityAdminApiClient, "delete_identity"
        ) as mock_delete_identity:
            mock_delete_identity.return_value = True

            deleted = test_identity_client.delete_identity_by_email_address(
                email_address="test-identity@test",
                ignore_missing=False,
            )
            self.assertTrue(deleted)

            mock_delete_identity.assert_called_once_with(
                identity_id="test-identity-id",
                ignore_missing=False,
            )

        mock_kratos_api_client.get.assert_called_once_with("/admin/identities")
        mock_kratos_response.json.assert_called_once()

    def test_delete_identity_by_email_address_not_found(self) -> None:
        """
        Test deleting an identity from its email address for an unknown identity
        """
        mock_kratos_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_kratos_response = unittest.mock.MagicMock(requests.Response)
        mock_kratos_response.status_code = http.HTTPStatus.NOT_FOUND.value
        mock_kratos_response.json.return_value = [
            {
                "id": "some-ignored-id",
                "traits": {
                    "email": "some-ignored@test",
                },
            },
        ]
        mock_kratos_api_client.get.return_value = mock_kratos_response

        test_identity_client = registration.IdentityAdminApiClient(
            kratos_api_client=mock_kratos_api_client,
            hydra_api_client=unittest.mock.MagicMock(registration.ApiRequestHelper),
        )

        with unittest.mock.patch.object(
            registration.IdentityAdminApiClient, "delete_identity"
        ) as mock_delete_identity:
            deleted = test_identity_client.delete_identity_by_email_address(
                email_address="test-identity@test",
                ignore_missing=False,
            )
            self.assertFalse(deleted)

            mock_delete_identity.assert_not_called()

        mock_kratos_api_client.get.assert_called_once_with("/admin/identities")
        mock_kratos_response.json.assert_called_once()

    def test_create_identity(self) -> None:
        """
        Test creating an identity
        """
        mock_kratos_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_kratos_response = unittest.mock.MagicMock(requests.Response)
        mock_kratos_response.status_code = http.HTTPStatus.CREATED.value
        mock_kratos_response.json.return_value = {"id": "test-identity-id"}
        mock_kratos_api_client.post.return_value = mock_kratos_response

        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)

        test_identity_client = registration.IdentityAdminApiClient(
            kratos_api_client=mock_kratos_api_client,
            hydra_api_client=mock_hydra_api_client,
        )
        identity_id = test_identity_client.create_identity(
            client_secret="test-secret",
            email_address="test-identity@test",
            password="test-password",
        )
        self.assertEqual(identity_id, "test-identity-id")

        mock_kratos_api_client.post.assert_called_once_with(
            path="/admin/identities",
            json={
                "schema_id": "default",
                "traits": {
                    "email": "test-identity@test",
                },
                "credentials": {
                    "password": {
                        "config": {
                            "password": "test-password",
                        },
                    },
                },
            },
            headers={"Accept": "application/json"},
            ignored_error_status=[http.HTTPStatus.CONFLICT],
        )
        mock_kratos_response.json.assert_called_once()
        mock_hydra_api_client.post.assert_called_once_with(
            path="/clients",
            json={
                "client_id": "test-identity-id",
                "client_name": "Machine-to-machine OAuth client for identity test-identity-id, "
                "administrative identity of Pixie Org registered for test-identity@test",
                "client_secret": "test-secret",
                "grant_types": ["client_credentials"],
                "scope": "vizier",
            },
        )

    def test_create_identity_exists(self) -> None:
        """
        Test creating an identity that clashes with an existing one in Kratos
        """
        mock_kratos_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_kratos_response = unittest.mock.MagicMock(requests.Response)
        mock_kratos_response.status_code = http.HTTPStatus.CONFLICT.value
        mock_kratos_api_client.post.return_value = mock_kratos_response

        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)

        test_identity_client = registration.IdentityAdminApiClient(
            kratos_api_client=mock_kratos_api_client,
            hydra_api_client=mock_hydra_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_identity_client.create_identity(
                client_secret="test-secret",
                email_address="test-identity@test",
                password="test-password",
            )
        self.assertEqual(str(err.exception), "Kratos identity exists")

        mock_kratos_api_client.post.assert_called_once_with(
            path="/admin/identities",
            json=unittest.mock.ANY,
            headers=unittest.mock.ANY,
            ignored_error_status=unittest.mock.ANY,
        )
        mock_hydra_api_client.post.assert_not_called()

    def test_create_identity_kratos_error_status(self) -> None:
        """
        Test creating an identity error: Unexpected Kratos API response
        """
        mock_kratos_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_kratos_response = unittest.mock.MagicMock(requests.Response)
        mock_kratos_response.status_code = http.HTTPStatus.OK.value
        mock_kratos_api_client.post.return_value = mock_kratos_response

        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)

        test_identity_client = registration.IdentityAdminApiClient(
            kratos_api_client=mock_kratos_api_client,
            hydra_api_client=mock_hydra_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_identity_client.create_identity(
                client_secret="test-secret",
                email_address="test-identity@test",
                password="test-password",
            )
        self.assertEqual(
            str(err.exception), "Unexpected Kratos API response (HTTP-200)"
        )

        mock_kratos_api_client.post.assert_called_once_with(
            path="/admin/identities",
            json=unittest.mock.ANY,
            headers=unittest.mock.ANY,
            ignored_error_status=unittest.mock.ANY,
        )
        mock_hydra_api_client.post.assert_not_called()

    def test_create_identity_kratos_missing_id(self) -> None:
        """
        Test creating an identity error: Unexpected Kratos API response (missing ID)
        """
        mock_kratos_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_kratos_response = unittest.mock.MagicMock(requests.Response)
        mock_kratos_response.status_code = http.HTTPStatus.CREATED.value
        mock_kratos_response.json.return_value = {}
        mock_kratos_api_client.post.return_value = mock_kratos_response

        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)

        test_identity_client = registration.IdentityAdminApiClient(
            kratos_api_client=mock_kratos_api_client,
            hydra_api_client=mock_hydra_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_identity_client.create_identity(
                client_secret="test-secret",
                email_address="test-identity@test",
                password="test-password",
            )
        self.assertEqual(
            str(err.exception), "Unexpected Kratos API response (missing ID)"
        )

        mock_kratos_api_client.post.assert_called_once_with(
            path="/admin/identities",
            json=unittest.mock.ANY,
            headers=unittest.mock.ANY,
            ignored_error_status=unittest.mock.ANY,
        )
        mock_hydra_api_client.post.assert_not_called()

    def test_create_identity_hydra_error_status(self) -> None:
        """
        Test creating an identity error: Unexpected Hydra API response
        """
        mock_kratos_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_kratos_response = unittest.mock.MagicMock(requests.Response)
        mock_kratos_response.status_code = http.HTTPStatus.CREATED.value
        mock_kratos_response.json.return_value = {"id": "test-identity-id"}
        mock_kratos_api_client.post.return_value = mock_kratos_response

        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_hydra_api_client.post.side_effect = registration.ApiError("test-error")

        test_identity_client = registration.IdentityAdminApiClient(
            kratos_api_client=mock_kratos_api_client,
            hydra_api_client=mock_hydra_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_identity_client.create_identity(
                client_secret="test-secret",
                email_address="test-identity@test",
                password="test-password",
            )
        self.assertEqual(str(err.exception), "test-error")

        mock_kratos_api_client.post.assert_called_once_with(
            path="/admin/identities",
            json=unittest.mock.ANY,
            headers=unittest.mock.ANY,
            ignored_error_status=unittest.mock.ANY,
        )
        mock_kratos_response.json.assert_called_once()
        mock_hydra_api_client.post.assert_called_once_with(
            path="/clients",
            json=unittest.mock.ANY,
        )


class TestOAuthApiClient(unittest.TestCase):
    """
    Test `OAuthApiClient`
    """

    @unittest.mock.patch.object(
        # NOTE: We use `unittest.mock.patch.object()` because a the mock module has a bug that
        # does not allow us to patch a staticmethod (and also doesn't allow `autospec=True`)
        #
        # https://bugs.python.org/issue23078
        #
        registration.ApiRequestHelper,
        "factory_by_known_api_name_from_env",
    )
    def test_factory_from_env(self, mock_api_request_helper_factory) -> None:
        """
        Test instantiating from the environment
        """
        # get a reference before mocking, so we can call the original function we want to test
        test_factory = registration.OAuthApiClient.factory_from_env

        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_oauth_client = unittest.mock.MagicMock(registration.OAuthApiClient)
        with unittest.mock.patch(
            "self_hosted_registration.registration.OAuthApiClient", autospec=True
        ) as mock_constructor:
            mock_api_request_helper_factory.return_value = mock_hydra_api_client
            mock_constructor.return_value = mock_oauth_client

            oauth_client = test_factory(env_variables_prefix="TEST_")
            self.assertIs(oauth_client, mock_oauth_client)

            mock_constructor.assert_called_once_with(api_client=mock_hydra_api_client)

        mock_api_request_helper_factory.assert_called_once_with(
            api_name=registration.KnownApiName.HYDRA_USER,
            env_variables_prefix="TEST_",
        )

    def test_get_access_token(self) -> None:
        """
        Test authenticating to get an access-token
        """
        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_hydra_response = unittest.mock.MagicMock(requests.Response)
        mock_hydra_response.status_code = http.HTTPStatus.OK.value
        mock_hydra_response.json.return_value = {"access_token": "test-access-token"}
        mock_hydra_api_client.post.return_value = mock_hydra_response

        test_oauth_client = registration.OAuthApiClient(
            api_client=mock_hydra_api_client,
        )
        access_token = test_oauth_client.get_access_token(
            username="test-username",
            password="test-password",
        )
        self.assertEqual(access_token, "test-access-token")

        mock_hydra_api_client.post.assert_called_once_with(
            path="/oauth2/token",
            auth=("test-username", "test-password"),
            data={
                "grant_type": "client_credentials",
                "scope": "vizier",
            },
            headers={"Accept": "application/json"},
        )

    def test_get_access_token_invalid_response(self) -> None:
        """
        Test authenticating to get an access-token: Invalid response from Hydra
        """
        mock_hydra_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_hydra_response = unittest.mock.MagicMock(requests.Response)
        mock_hydra_response.status_code = http.HTTPStatus.OK.value
        mock_hydra_response.json.return_value = {}
        mock_hydra_api_client.post.return_value = mock_hydra_response

        test_oauth_client = registration.OAuthApiClient(
            api_client=mock_hydra_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_oauth_client.get_access_token(
                username="test-username",
                password="test-password",
            )
        self.assertEqual(str(err.exception), "Invalid Hydra response")

        mock_hydra_api_client.post.assert_called_once()


class TestPixieRegistrationApiClient(unittest.TestCase):
    """
    Test `PixieRegistrationApiClient`
    """

    @unittest.mock.patch.object(
        # NOTE: We use `unittest.mock.patch.object()` because a the mock module has a bug that
        # does not allow us to patch a staticmethod (and also doesn't allow `autospec=True`)
        #
        # https://bugs.python.org/issue23078
        #
        registration.ApiRequestHelper,
        "factory_by_known_api_name_from_env",
    )
    def test_factory_from_env(self, mock_api_request_helper_factory) -> None:
        """
        Test instantiating from the environment
        """
        # get a reference before mocking, so we can call the original function we want to test
        test_factory = registration.PixieRegistrationApiClient.factory_from_env

        mock_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_pixie_client = unittest.mock.MagicMock(
            registration.PixieRegistrationApiClient
        )
        with unittest.mock.patch(
            "self_hosted_registration.registration.PixieRegistrationApiClient",
            autospec=True,
        ) as mock_constructor:
            mock_api_request_helper_factory.return_value = mock_api_client
            mock_constructor.return_value = mock_pixie_client

            pixie_client = test_factory(env_variables_prefix="TEST_")
            self.assertIs(pixie_client, mock_pixie_client)

            mock_constructor.assert_called_once_with(api_client=mock_api_client)

        mock_api_request_helper_factory.assert_called_once_with(
            api_name=registration.KnownApiName.PIXIE,
            env_variables_prefix="TEST_",
        )

    def test_api_request_without_login(self) -> None:
        """
        Test performing an API call (create_api_key) without a prior login
        """
        mock_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)

        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=mock_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_pixie_client.create_api_key()
        self.assertEqual(str(err.exception), "Missing Pixie API login")

        mock_api_client.post.assert_not_called()

    def test_login(self) -> None:
        """
        Test login
        """
        mock_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_response = unittest.mock.MagicMock(requests.Response)
        mock_response.status_code = http.HTTPStatus.OK.value
        mock_response.json.return_value = {
            "token": "test-token",
            "userInfo": {
                "userID": "test-user-id",
                "email": "test-email",
            },
            "orgInfo": {
                "orgID": "test-org-id",
                "orgName": "test-org-name",
            },
        }
        mock_api_client.post.side_effect = [
            mock_response,
            Exception("Expected-create_api_key-error"),
        ]

        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=mock_api_client,
        )
        test_pixie_client.login(access_token="test-access-token")

        # now we test that the retrieved token is used in subsequent calls. This fails just
        # because we don't fully mock out its logic (as it is tested separately). The important
        # piece is that the API client is called with the token (see below)
        with self.assertRaises(Exception) as err:
            test_pixie_client.create_api_key()
        self.assertEqual(str(err.exception), "Expected-create_api_key-error")

        mock_api_client.post.assert_has_calls(
            [
                unittest.mock.call(
                    path="/api/auth/login",
                    json={
                        "accessToken": "test-access-token",
                    },
                ),
                unittest.mock.call(
                    path="/api/graphql",
                    # NOTE: This is the important piece - we propagate the bearer auth token
                    headers={"authorization": "bearer test-token"},
                    json=unittest.mock.ANY,
                ),
            ]
        )

    def test_login_missing_data(self) -> None:
        """
        Test login: invalid response from API
        """
        mock_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_response = unittest.mock.MagicMock(requests.Response)
        mock_response.status_code = http.HTTPStatus.OK.value
        mock_response.json.return_value = {}
        mock_api_client.post.return_value = mock_response

        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=mock_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_pixie_client.login(access_token="test-access-token")
        self.assertEqual(str(err.exception), "Pixie login did not return expected data")

        mock_api_client.post.assert_called_once_with(
            path="/api/auth/login",
            json={
                "accessToken": "test-access-token",
            },
        )
        mock_response.json.assert_called_once()

    def test_signup(self) -> None:
        """
        Test signup
        """
        mock_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)

        mock_signup_response = unittest.mock.MagicMock(requests.Response)
        mock_signup_response.status_code = http.HTTPStatus.OK.value
        mock_signup_response.json.return_value = {"token": "test-token"}

        mock_authorized_response = unittest.mock.MagicMock(requests.Response)
        mock_authorized_response.status_code = http.HTTPStatus.OK.value

        mock_api_client.post.side_effect = [
            mock_signup_response,
            mock_authorized_response,
        ]

        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=mock_api_client,
        )
        with unittest.mock.patch.object(
            # we mock out the internal graphql API invocation as that is tested separately. It
            # would make this test too complex/do too many things
            registration.PixieRegistrationApiClient,
            "_do_graphql_operation",
        ) as mock_do_graphql_operation:
            test_pixie_client.signup(
                org_name="test_org",
                access_token="test-access-token",
            )
            mock_do_graphql_operation.assert_called_once_with(
                operation_name="CreateOrgFromSetupOrgPage",
                variables={
                    "orgName": "test_org",
                },
                query=unittest.mock.ANY,
            )

        mock_api_client.post.assert_has_calls(
            [
                unittest.mock.call(
                    path="/api/auth/signup",
                    json={
                        "accessToken": "test-access-token",
                    },
                    ignored_error_status=[http.HTTPStatus.INTERNAL_SERVER_ERROR],
                ),
                unittest.mock.call(
                    path="/api/authorized",
                    headers={"authorization": "bearer test-token"},
                ),
            ]
        )

    def test_signup_missing_data(self) -> None:
        """
        Test signup: invalid response from API
        """
        mock_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_response = unittest.mock.MagicMock(requests.Response)
        mock_response.status_code = http.HTTPStatus.OK.value
        mock_response.json.return_value = {}
        mock_api_client.post.return_value = mock_response

        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=mock_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_pixie_client.signup(
                org_name="test_org",
                access_token="test-access-token",
            )
        self.assertEqual(str(err.exception), "Pixie Org signup did not return token")

        mock_api_client.post.assert_called_once_with(
            path="/api/auth/signup",
            json={
                "accessToken": "test-access-token",
            },
            ignored_error_status=[http.HTTPStatus.INTERNAL_SERVER_ERROR],
        )
        mock_response.json.assert_called_once()

    def test_signup_api_error(self) -> None:
        """
        Test signup: unexpected API error
        """
        mock_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_response = unittest.mock.MagicMock(requests.Response)
        mock_response.status_code = http.HTTPStatus.INTERNAL_SERVER_ERROR.value
        mock_response.text = "test error"
        mock_api_client.post.return_value = mock_response

        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=mock_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_pixie_client.signup(
                org_name="test_org",
                access_token="test-access-token",
            )
        self.assertEqual(str(err.exception), "Pixie Org signup failed")

        mock_api_client.post.assert_called_once_with(
            path="/api/auth/signup",
            json={
                "accessToken": "test-access-token",
            },
            ignored_error_status=[http.HTTPStatus.INTERNAL_SERVER_ERROR],
        )

    def test_signup_existing_org(self) -> None:
        """
        Test signup: Pixie Org exists already
        """
        mock_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_response = unittest.mock.MagicMock(requests.Response)
        mock_response.status_code = http.HTTPStatus.INTERNAL_SERVER_ERROR.value
        mock_response.text = "Failed to signup: cannot create duplicate user"
        mock_api_client.post.return_value = mock_response

        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=mock_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_pixie_client.signup(
                org_name="test_org",
                access_token="test-access-token",
            )
        self.assertEqual(str(err.exception), "Pixie Org signup failed: Org exists")

        mock_api_client.post.assert_called_once_with(
            path="/api/auth/signup",
            json={
                "accessToken": "test-access-token",
            },
            ignored_error_status=[http.HTTPStatus.INTERNAL_SERVER_ERROR],
        )

    def test_signup_invalid_org_name(self) -> None:
        """
        Test signup using an invalid Pixie Org name
        """
        mock_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)

        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=mock_api_client,
        )
        with self.assertRaises(registration.Error) as err:
            test_pixie_client.signup(
                org_name="this is not a valid Pixie Org name",
                access_token="test-access-token",
            )
        self.assertEqual(str(err.exception), "Invalid Pixie Org name")

        mock_api_client.post.assert_not_called()

    def test_create_api_key(self) -> None:
        """
        Test create_api_key
        """
        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=unittest.mock.MagicMock(registration.ApiRequestHelper),
        )
        with unittest.mock.patch.object(
            # we mock out the internal API POST invocation to keep complexity of this
            # test reasonable
            registration.PixieRegistrationApiClient,
            "_api_post",
        ) as mock_api_post:
            mock_create_response = unittest.mock.MagicMock(requests.Response)
            mock_create_response.json.return_value = {
                "data": {
                    "CreateAPIKey": {
                        "id": "test-api-key-id",
                    },
                },
            }
            mock_get_response = unittest.mock.MagicMock(requests.Response)
            mock_get_response.json.return_value = {
                "data": {
                    "apiKey": {
                        "id": "test-api-key-id",
                        "key": "test-api-key-secret",
                        "desc": "",
                    },
                },
            }

            mock_api_post.side_effect = [
                mock_create_response,
                mock_get_response,
            ]

            api_key_id, api_key_secret = test_pixie_client.create_api_key()
            self.assertEqual(api_key_id, "test-api-key-id")
            self.assertEqual(api_key_secret, "test-api-key-secret")

            mock_api_post.assert_has_calls(
                [
                    unittest.mock.call(
                        path="/api/graphql",
                        json={
                            "operationName": "CreateAPIKeyFromAdminPage",
                            "variables": {},
                            "query": unittest.mock.ANY,
                        },
                    ),
                    unittest.mock.call(
                        path="/api/graphql",
                        json={
                            "operationName": "getAPIKey",
                            "variables": {
                                "id": "test-api-key-id",
                            },
                            "query": unittest.mock.ANY,
                        },
                    ),
                ]
            )

    def test_create_api_key_missing_create_data(self) -> None:
        """
        Test create_api_key: missing data in API call to create key
        """
        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=unittest.mock.MagicMock(registration.ApiRequestHelper),
        )
        with unittest.mock.patch.object(
            # we mock out the internal API POST invocation to keep complexity of this
            # test reasonable
            registration.PixieRegistrationApiClient,
            "_do_graphql_operation",
        ) as mock_do_graphql_operation:
            mock_do_graphql_operation.return_value = {
                "CreateAPIKey": "invalid",
            }

            with self.assertRaises(registration.ApiError) as err:
                test_pixie_client.create_api_key()
            self.assertEqual(
                str(err.exception),
                "Response from CreateAPIKeyFromAdminPage is invalid: string indices must "
                "be integers",
            )

            mock_do_graphql_operation.assert_called_once_with(
                operation_name="CreateAPIKeyFromAdminPage",
                query=unittest.mock.ANY,
            )

    def test_create_api_key_missing_get_data(self) -> None:
        """
        Test create_api_key: missing data in API call to get key
        """
        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=unittest.mock.MagicMock(registration.ApiRequestHelper),
        )
        with unittest.mock.patch.object(
            # we mock out the internal API POST invocation to keep complexity of this
            # test reasonable
            registration.PixieRegistrationApiClient,
            "_do_graphql_operation",
        ) as mock_do_graphql_operation:
            mock_do_graphql_operation.side_effect = [
                {
                    "CreateAPIKey": {
                        "id": "test-api-key-id",
                    },
                },
                {
                    "apiKey": {
                        "missing": "data",
                    },
                },
            ]

            with self.assertRaises(registration.ApiError) as err:
                test_pixie_client.create_api_key()
            self.assertEqual(
                str(err.exception), "Response from getAPIKey invalid: 'key'"
            )

            mock_do_graphql_operation.assert_has_calls(
                [
                    unittest.mock.call(
                        operation_name="CreateAPIKeyFromAdminPage",
                        query=unittest.mock.ANY,
                    ),
                    unittest.mock.call(
                        operation_name="getAPIKey",
                        variables={
                            "id": "test-api-key-id",
                        },
                        query=unittest.mock.ANY,
                    ),
                ]
            )

    def test_graphql_missing_data(self) -> None:
        """
        Test handling of missing data in response to graphql API
        """
        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=unittest.mock.MagicMock(registration.ApiRequestHelper),
        )
        with unittest.mock.patch.object(
            # we mock out the internal API POST invocation to keep complexity of this
            # test reasonable
            registration.PixieRegistrationApiClient,
            "_api_post",
        ) as mock_api_post:
            mock_create_response = unittest.mock.MagicMock(requests.Response)
            mock_create_response.json.return_value = {"test": "incomplete"}
            mock_api_post.return_value = mock_create_response

            with self.assertRaises(registration.ApiError) as err:
                test_pixie_client.create_api_key()
            self.assertEqual(str(err.exception), "GraphQL API response invalid: 'data'")

            mock_api_post.assert_called_once_with(
                path="/api/graphql",
                json=unittest.mock.ANY,
            )

    def test_create_deployment_key(self) -> None:
        """
        Test create_deployment_key
        """
        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=unittest.mock.MagicMock(registration.ApiRequestHelper),
        )
        with unittest.mock.patch.object(
            # we mock out the internal helper invocation to keep complexity of this
            # test reasonable
            registration.PixieRegistrationApiClient,
            "_create_id_entity",
        ) as mock_create_id_entity:
            mock_create_id_entity.return_value = (
                "test-deployment-key-id",
                "test-deployment-key-secret",
            )
            api_key_id, api_key_secret = test_pixie_client.create_deployment_key()
            self.assertEqual(api_key_id, "test-deployment-key-id")
            self.assertEqual(api_key_secret, "test-deployment-key-secret")

            mock_create_id_entity.assert_called_once_with(
                entity_type_name="deployment key",
                create_operation_name="CreateDeploymentKeyFromAdminPage",
                create_query_name="CreateDeploymentKey",
                get_operation_name="getDeploymentKey",
                get_query_name="deploymentKey",
            )

    def test_configure_org_settings(self) -> None:
        """
        Test configure_org_settings
        """
        mock_api_client = unittest.mock.MagicMock(registration.ApiRequestHelper)
        mock_response = unittest.mock.MagicMock(requests.Response)
        mock_response.status_code = http.HTTPStatus.OK.value
        mock_response.json.return_value = {
            "token": "test-token",
            "userInfo": {
                "userID": "test-user-id",
                "email": "test-email",
            },
            "orgInfo": {
                "orgID": "test-org-id",
                "orgName": "test-org-name",
            },
        }
        mock_api_client.post.return_value = mock_response

        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=mock_api_client,
        )
        test_pixie_client.login(access_token="test-access-token")

        with unittest.mock.patch.object(
            # we mock out the internal graphql API invocation as that is tested separately. It
            # would make this test too complex/do too many things
            registration.PixieRegistrationApiClient,
            "_do_graphql_operation",
        ) as mock_do_graphql_operation:
            test_pixie_client.configure_org_settings(enable_user_approvals=True)

            mock_do_graphql_operation.assert_called_once_with(
                operation_name="UpdateOrgApprovalSetting",
                variables={
                    "orgID": "test-org-id",
                    "enableApprovals": True,
                },
                query=unittest.mock.ANY,
            )

    def test_configure_user_settings(self) -> None:
        """
        Test configure_user_settings
        """
        test_pixie_client = registration.PixieRegistrationApiClient(
            api_client=unittest.mock.MagicMock(registration.ApiRequestHelper),
        )
        with unittest.mock.patch.object(
            # we mock out the internal helper invocation to keep complexity of this
            # test reasonable
            registration.PixieRegistrationApiClient,
            "_do_graphql_operation",
        ) as mock_do_graphql_operation:
            test_pixie_client.configure_user_settings(
                enable_analytics=True,
            )

            mock_do_graphql_operation.assert_called_once_with(
                operation_name="UpdateUserAnalyticOptOut",
                variables={
                    "analyticsOptout": False,
                },
                query=unittest.mock.ANY,
            )


if __name__ == "__main__":
    unittest.main()
