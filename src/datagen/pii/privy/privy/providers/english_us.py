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
import string
import random
import baluhn
from decimal import Decimal
from faker import Faker
from faker_airtravel import AirTravelProvider
from privy.providers.generic import GenericProvider, Provider


# English United States - inherits standard, region-agnostic methods
class English_US(GenericProvider):
    def __init__(self, pii_types=None, locale="en_US"):
        # initialize standard, region-agnostic methods
        super().__init__()
        # initialize Faker instance with specific Faker locale
        f = Faker([locale])
        f.add_provider(AirTravelProvider)
        custom = self.CustomProviders(f)
        self.f = f
        # define language/region-specific providers
        self.pii_providers = [
            # ------ Names ------
            Provider(
                name="person",
                aliases=set([
                    "full name",
                    "account name",
                    "artist name",
                    "contact name",
                    "login name",
                    "user name",
                    "customer",
                    "user",
                    "target user name",
                    "buyer user name",
                    "shareholder",
                    "owner",
                ]),
                generator=f.name),
            Provider(
                "name_male",
                set([
                    "full name male",
                ]),
                f.name_male),
            Provider(
                "name_female",
                set([
                    "full name female",
                ]),
                f.name_female),
            Provider(
                "first_name",
                set([
                    "given name",
                    "middle name",
                ]),
                f.first_name),
            Provider(
                "first_name_nonbinary",
                set([
                    "given name nonbinary",
                ]),
                f.first_name_nonbinary),
            Provider(
                "first_name_male",
                set([
                    "given name male",
                ]),
                f.first_name_male),
            Provider(
                "first_name_female",
                set([
                    "given name female",
                ]),
                f.first_name_male),
            Provider(
                "last_name",
                set([
                    "family name",
                ]),
                f.last_name),
            Provider(
                "last_name_male",
                set([
                    "family name male",
                ]),
                f.last_name_male),
            Provider(
                "last_name_female",
                set([
                    "family fename",
                ]),
                f.last_name_female),
            Provider(
                "prefix",
                set(),
                f.prefix),
            Provider(
                "prefix_male",
                set(),
                f.prefix_male),
            Provider(
                "prefix_female",
                set(),
                f.prefix_female),
            Provider(
                "company",
                set([
                    "organization",
                    "company name",
                    "department",
                    "manufacturer",
                    "client",
                    "dba",
                    "doing business as",
                    "business name",
                    "business",
                ]),
                f.company),
            Provider(
                "nationality",
                set(),
                f.country),
            Provider(
                "nation_woman",
                set(),
                f.country),
            Provider(
                "nation_man",
                set(),
                f.country),
            Provider(
                "nation_plural",
                set(),
                f.country),
            # ------ Location ------
            Provider(
                "address",
                set([
                    "home",
                    "work",
                    "venue",
                    "place",
                    "spot",
                    "facility",
                ]),
                f.address),
            Provider(
                "street_address",
                set([
                    "street",
                    "avenue",
                    "alley",
                ]),
                f.street_address),
            Provider(
                "country",
                set([
                    "destination",
                    "origin",
                ]),
                f.country),
            Provider(
                "country_code",
                set([
                    "to country code",
                    "from country code",
                    "phone country code",
                ]),
                f.country_code),
            Provider(
                "state",
                set([
                    "province",
                    "region",
                    "federal state",
                ]),
                f.state),
            Provider(
                "city",
                set([
                    "bank city",
                    "municipality",
                    "urban area",
                ]),
                f.city),
            Provider(
                "postcode",
                set([
                    "post code",
                    "postal code",
                ]),
                f.postcode),
            Provider(
                "building_number",
                set([
                    "house",
                    "building",
                    "apartment",
                ]),
                f.building_number),
            Provider(
                "street_name",
                set([
                    "road",
                    "lane",
                    "drive",
                ]),
                f.street_name),
            Provider(
                "coordinate",
                set([
                    "location",
                    "position",
                ]),
                f.coordinate,
                Decimal),
            Provider(
                "latitude",
                set([
                    "lat",
                ]),
                f.latitude,
                Decimal),
            Provider(
                "longitude",
                set([
                    "lon",
                ]),
                f.longitude,
                Decimal),
            Provider(
                "airport_name",
                set([
                    "airport",
                ]),
                f.airport_name),
            Provider(
                "airport_iata",
                set([
                    "airport code",
                    "origin airport code",
                    "arrival airport code",
                    "destination airport code",
                ]),
                f.airport_iata),
            Provider(
                "airport_icao",
                set(),
                f.airport_icao),
            Provider(
                "airline",
                set(["arline name"]),
                f.airline),
            # ------ Financial ------
            Provider(
                "bban",
                set([
                    "bank_account_number",
                    "bank account",
                    "bic",
                ]),
                f.bban),
            Provider(
                "aba",
                set([
                    "routing_transit_number",
                    "routing number",
                ]),
                f.aba),
            Provider(
                "iban",
                set([
                    "international_bank_account_number",
                ]),
                f.iban),
            Provider(
                "credit_card_number",
                set([
                    "credit card",
                    "debit card",
                    "master card",
                    "visa",
                    "american express",
                ]),
                f.credit_card_number),
            Provider(
                "credit_card_expire",
                set([
                    "credit_card_expiration_date",
                    "expiration date",
                    "expiration",
                    "expires",
                ]),
                f.credit_card_expire),
            Provider(
                "swift",
                set([
                    "swift code",
                ]),
                f.swift),
            Provider(
                "currency_code",
                set([
                    "fare currency",
                    "currency",
                ]),
                f.currency_code),
            # ------ Time ------
            Provider(
                "day_of_week",
                set([
                    "week day",
                ]),
                f.day_of_week),
            Provider(
                "date_of_birth",
                set([
                    "birth day",
                    "birth date",
                ]),
                f.date_of_birth),
            Provider(
                "date",
                set([
                    "modified date",
                    "from booking date",
                    "to booking date",
                    "open date",
                    "to date",
                    "published",
                    "day",
                    "departure date",
                    "return date",
                    "start date",
                    "end date",
                    "travel date",
                    "from date",
                    "install date",
                ]),
                f.date),
            Provider(
                "year",
                set([
                    "birth year",
                ]),
                f.year),
            Provider(
                "month",
                set([
                    "birth month",
                ]),
                f.month,
            ),
            Provider(
                "date_time",
                set([
                    "from statement date time",
                    "to statement date time",
                    "time stamp",
                    "last timestamp",
                    "last modified",
                    "modified after",
                    "modified before",
                    "from timestamp",
                    "to timestamp",
                    "end time",
                    "start time",
                    "last updated",
                    "created",
                    "unix time",
                    "start",
                    "end",
                ]),
                f.iso8601),
            # ------ Identification ------
            Provider(
                "ssn",
                set([
                    "social_security_number",
                    "id number",
                    "id card",
                ]),
                f.ssn),
            Provider(
                "passport",
                set([
                    "passport",
                    "passport number",
                    "document number",
                    "identity document",
                    "national identity",
                ]),
                custom.passport),
            Provider(
                "drivers_license",
                set([
                    "driving license",
                    "driver's license",
                    "driver license",
                ]),
                custom.drivers_license),
            Provider(
                "license_plate",
                set([
                    "lic plate",
                ]),
                f.license_plate),
            # ------ Contact Info ------
            Provider(
                "email",
                set([
                    "email address",
                    "contact email",
                    "to contact",
                ]),
                f.email),
            Provider(
                "phone_number",
                set([
                    "phone",
                    "contact phone",
                    "associate phone number",
                ]),
                f.phone_number),
            # ------ Demographic ------
            Provider(
                "gender",
                set([
                    "sexuality",
                    "sex",
                ]),
                custom.gender
            ),
            Provider(
                "job",
                set([
                    "occupation",
                    "profession",
                    "employment",
                    "vocation",
                    "career",
                ]),
                f.job,
            ),
            # ------ Internet / Devices ------
            Provider(
                "domain_name",
                set([
                    "domain",
                ]),
                f.domain_name
            ),
            Provider(
                "url",
                set([
                    "website",
                    "repository",
                    "url",
                    "site",
                    "host name",
                ]),
                f.url
            ),
            Provider(
                "ip_address",
                set([
                    "ipv4",
                    "ipv6",
                ]),
                random.choice([f.ipv6, f.ipv4])
            ),
            Provider(
                "mac_address",
                set([
                    "device mac",
                    "mac_address__nie",
                ]),
                custom.mac_address,
            ),
            Provider(
                "imei",
                set([
                    "international mobile equipment identity"
                ]),
                custom.imei,
            ),
            Provider(
                "password",
                set([
                    "key password",
                    "key store password",
                    "current password",
                ]),
                f.password,
            ),
        ]
        self.nonpii_providers = [
            Provider(
                name="string",
                aliases=set(["string", "text", "message"]),
                generator=custom.string,
                type_=str,
            ),
            Provider(
                "boolean",
                set(["bool"]),
                f.boolean,
            ),
            Provider(
                "color",
                set(["hue", "colour"]),
                f.color,
            ),
            Provider(
                "random_number",
                set(["integer", "int", "number", "to number", "from number"]),
                f.random_number,
                int,
            ),
            Provider(
                "sha1",
                set(["signature sha1", "serial", "app key",
                    "id", "org id", "statement id", "device id"]),
                f.sha1,
            ),
        ]
        # filter providers, marking providers matching given pii_types as pii
        self.filter_providers(pii_types)

    class CustomProviders:
        def __init__(self, faker):
            self.f = faker

        def mac_address(self) -> str:
            pattern = random.choice(
                [
                    "^^:^^:^^:^^:^^:^^",
                    "^^-^^-^^-^^-^^-^^",
                    "^^ ^^ ^^ ^^ ^^ ^^",
                ]
            )
            return self.f.hexify(pattern)

        def imei(self) -> str:
            imei = self.f.numerify(text="##-######-######-#")
            while baluhn.verify(imei.replace("-", "")) is False:
                imei = self.f.numerify(text="##-######-######-#")
            return imei

        def boolean(self) -> str:
            return random.choice(["True", "False"])

        def gender(self) -> str:
            return random.choice(["Male", "Female", "Other"])

        def passport(self) -> str:
            # US Passports consist of 1 letter or digit followed by 8-digits
            return self.f.bothify(text=random.choice(["?", "#"]) + "########")

        def drivers_license(self) -> str:
            # US driver's licenses consist of 9 digits (patterns vary by state)
            return self.f.numerify(text="### ### ###")

        def string(self) -> str:
            """generate a random string of characters, words, and numbers"""
            def sample(text, low, high, space=False):
                """sample randomly from input text with a minimum length of low and maximum length of high"""
                space = " " if space else ""
                return space.join(random.sample(text, random.randint(low, high)))

            characters = sample(string.ascii_letters, 1, 10)
            numbers = sample(string.digits, 1, 10)
            characters_and_numbers = sample(string.ascii_letters + string.digits, 1, 10)
            combined = self.f.words(nb=3) + [characters, numbers, characters_and_numbers]
            return sample(combined, 0, 6, True)
