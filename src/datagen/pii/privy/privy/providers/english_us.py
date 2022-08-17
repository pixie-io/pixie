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
import re
import string
import random
import baluhn
from collections import namedtuple
from faker import Faker
from faker_airtravel import AirTravelProvider
from privy.providers.generic import GenericProvider


# English United States - inherits standard, region-agnostic methods
class English_US(GenericProvider):
    def __init__(self):
        # initialize standard, region-agnostic methods
        super().__init__()
        # initialize Faker instance with specific Faker locale
        f = Faker(["en_US"])
        f.add_provider(AirTravelProvider)
        custom = self.CustomProviders(f)
        self.f = f
        # initialize named tuple to hold matched pii provider data for a given pii label
        self.PII = namedtuple("PII", ["category", "label", "value"])
        self.NonPII = namedtuple("NonPII", ["label", "value"])
        # map custom, language/region-specific nonpii keywords to providers
        # labels matched with case insensitive regex and space separated
        # (different delimiters are inserted at runtime)
        self.pii_label_to_provider = {
            # ------ Names ------
            "names": {
                "account name": f.name,
                "artist name": f.name,
                "contact name": f.name,
                "login name": f.name,
                "user name": f.name,
                "customer": f.name,
                "user": f.name,
                "target user name": f.name,
                "buyer user name": f.name,
                "full name": f.name,
                "shareholder": f.name,
                "owner": f.name,
                "first name": f.first_name,
                "first": f.first_name,
                "given name": f.first_name,
                "middle name": f.first_name,
                "middle initial": f.first_name,
                "last name": f.last_name,
                "last": f.last_name,
                "family name": f.last_name,
                "company": f.company,
                "department": f.company,
                "manufacturer": f.company,
                "client": f.company,
                "organization": f.company,
                "dba": f.company,
                "doing business as": f.company,
                "business name": f.company,
                "business": f.company,
            },
            # ------ Location ------
            "location": {
                "address": f.address,
                "home": f.address,
                "work": f.address,
                "venue": f.address,
                "place": f.address,
                "spot": f.address,
                "facility": f.address,
                "country": f.country,
                "destination": f.country,
                "origin": f.country,
                "nationality": f.country,
                "country code": f.country_code,
                "to country code": f.country_code,
                "from country code": f.country_code,
                "phone country code": f.country_code,
                "state": f.state,
                "province": f.state,
                "region": f.state,
                "federal state": f.state,
                "city": f.city,
                "bank city": f.city,
                "municipality": f.city,
                "urban area": f.city,
                "post code": f.postcode,
                "zip code": f.postcode,
                "area code": f.postcode,
                "zip": f.postcode,
                "building number": f.building_number,
                "house": f.building_number,
                "building": f.building_number,
                "apartment": f.building_number,
                "street": f.street_address,
                "road": f.street_name,
                "street address": f.street_address,
                "lane": f.street_name,
                "drive": f.street_name,
                "avenue": f.street_address,
                "alley": f.street_address,
                "location": f.coordinate,
                "coordinate": f.coordinate,
                "position": f.coordinate,
                "latitude": f.latitude,
                "lat": f.latitude,
                "longitude": f.longitude,
                "lon": f.longitude,
                "airport": f.airport_name,
                "airport name": f.airport_name,
                "airport iata": f.airport_iata,
                "airport icao": f.airport_icao,
                "airline": f.airline,
                "airport code": f.airport_iata,
                "origin airport code": f.airport_iata,
                "arrival airport code": f.airport_iata,
                "destination airport code": f.airport_iata,
            },
            # ------ Financial ------
            "financial": {
                "bank account": f.bban,
                "bban": f.bban,
                "bic": f.bban,
                "aba": f.aba,
                "routing transit number": f.aba,
                "routing number": f.aba,
                "iban": f.iban,
                "international bank acccount number": f.iban,
                "credit card": f.credit_card_number,
                "debit card": f.credit_card_number,
                "master card": f.credit_card_number,
                "visa": f.credit_card_number,
                "american express": f.credit_card_number,
                "expiration date": f.credit_card_expire,
                "expiration": f.credit_card_expire,
                "expires": f.credit_card_expire,
                "swift": f.swift,
                "balance": f.random_number,
                "fare": f.random_number,
                "net fare": f.random_number,
                "amount": f.random_number,
                "credit": f.random_number,
                "currency": f.currency_code,
                "fare-currency": f.currency_code,
            },
            # ------ Time ------
            "time": {
                "date": f.date,
                "modified date": f.date,
                "from booking date": f.date,
                "to booking date": f.date,
                "open date": f.date,
                "to date": f.date,
                "published": f.date,
                "day": f.date,
                "departure date": f.date,
                "return date": f.date,
                "start date": f.date,
                "end date": f.date,
                "travel date": f.date,
                "from date": f.date,
                "from statement date time": f.iso8601,
                "to statement date time": f.iso8601,
                "install date": f.date,
                "birth day": f.date,
                "birth date": f.date,
                "year": f.year,
                "birth year": f.year,
                "month": f.month,
                "birth month": f.month,
                "time stamp": f.iso8601,
                "last timestamp": f.iso8601,
                "last modified": f.iso8601,
                "modified after": f.iso8601,
                "modified before": f.iso8601,
                "from timestamp": f.iso8601,
                "to timestamp": f.iso8601,
                "end time": f.iso8601,
                "start time": f.iso8601,
                "last updated": f.iso8601,
                "created": f.iso8601,
                "unix time": f.iso8601,
                "start": f.iso8601,
                "end": f.iso8601,
            },
            # ------ Identification ------
            "identification": {
                "social security number": f.ssn,
                "id number": f.ssn,
                "id card": f.ssn,
                "passport": custom.us_passport,
                "document number": custom.us_passport,
                "identity document": custom.us_passport,
                "national identity": custom.us_passport,
                "driving license": custom.us_drivers_license,
                "drivers license": custom.us_drivers_license,
                "driver's license": custom.us_drivers_license,
                "license plate": f.license_plate,
                "lic plate": f.license_plate,
                "taxId": custom.alphanum,
            },
            # ------ Contact Info ------
            "contact": {
                "email": f.email,
                "contact email": f.email,
                "to contact": f.email,
                "phone": f.phone_number,
                "contact phone": f.phone_number,
                "phone number": f.phone_number,
                "associate phone number": f.phone_number,
            },
            # ------ Demographic ------
            "demographic": {
                "gender": custom.gender,
                "sexuality": custom.gender,
                "sex": custom.gender,
                "occupation": f.job,
                "job": f.job,
                "profession": f.job,
                "employment": f.job,
                "vocation": f.job,
                "career": f.job,
            },
            # ------ Internet / Devices ------
            "internet": {
                "api key": f.sha1,
                "app key": f.sha1,
                "uuid": f.uuid4,
                "signature sha1": f.sha1,
                "serial": f.sha1,
                "website": f.url,
                "domain name": f.domain_name,
                "repository": f.url,
                "url": f.url,
                "site": f.url,
                "host name": f.url,
                "ipv4": f.ipv4,
                "ipv6": f.ipv6,
                "ip address": random.choice([f.ipv6, f.ipv4]),
                "mac address": custom.mac_address,
                "device mac": custom.mac_address,
                "imei": custom.imei,
                "uri path": f.uri_path,
                "password": f.password,
                "key password": f.password,
                "current password": f.password,
                "key store password": f.password,
                "file": f.file_name,
                "path": f.file_path,
                "file path": f.file_path,
            },
        }
        self.nonpii_label_to_provider = {
            "alphanumeric": custom.alphanum,
            "id": custom.alphanum,
            "org id": custom.alphanum,
            "statement id": custom.alphanum,
            "string": custom.string,
            "bool": custom.boolean,
            "boolean": custom.boolean,
            "text": f.text,
            "message": f.text,
            "number": f.random_number,
            "from number": f.random_number,
            "to number": f.random_number,
            "int": f.random_number,
            "integer": f.random_number,
        }

    def get_pii_categories(self):
        return self.pii_label_to_provider.keys()

    def get_category(self, category):
        return self.pii_label_to_provider[category]

    def get_delimited(self, label):
        """get a list of copies of the input label with different delimiters"""
        label_delimited = [
            label,
            label.replace(" ", "-"),
            label.replace(" ", "_"),
            label.replace(" ", "__"),
            label.replace(" ", "."),
            label.replace(" ", ":"),
            label.replace(" ", ""),
        ]
        return label_delimited

    def get_pii(self, name):
        if not name:
            return
        for category in self.get_pii_categories():
            for label, provider in self.pii_label_to_provider[category].items():
                # check if name at least partially matches pii label
                # for multiword labels, check versions of the label with different delimiters
                label_delimited = self.get_delimited(label)
                for lbl in label_delimited:
                    if re.match(lbl, name, re.IGNORECASE):
                        return self.PII(category, label, str(provider()))

    def get_nonpii(self, name):
        if not name:
            return
        for label, provider in self.nonpii_label_to_provider.items():
            # check if name at least partially matches nonpii label
            # for multiword labels, check versions of the label with different delimiters
            label_delimited = self.get_delimited(label)
            for lbl in label_delimited:
                if re.match(lbl, name, re.IGNORECASE):
                    return self.NonPII(label, str(provider()))

    def get_random_pii(self):
        category = random.choice(list(self.get_pii_categories()))
        label = random.choice(
            list(self.get_category(category).keys()))
        return self.get_pii(label)

    def sample_pii(self, percent):
        # randomly select a category
        category = random.choice(list(self.get_pii_categories()))
        labels = random.sample(
            list(self.get_category(category).keys()),
            round(
                len(self.get_category(category).keys()) * percent),
        )
        return [self.get_pii(label) for label in labels]

    class CustomProviders:
        def __init__(self, faker):
            self.f = faker

        def mac_address(self):
            pattern = random.choice(
                [
                    "^^:^^:^^:^^:^^:^^",
                    "^^-^^-^^-^^-^^-^^",
                    "^^ ^^ ^^ ^^ ^^ ^^",
                ]
            )
            return self.f.hexify(pattern)

        def imei(self):
            imei = self.f.numerify(text="##-######-######-#")
            while baluhn.verify(imei.replace("-", "")) is False:
                imei = self.f.numerify(text="##-######-######-#")
            return imei

        def boolean(self):
            return random.choice(["True", "False"])

        def gender(self):
            return random.choice(["Male", "Female", "Other"])

        def us_passport(self):
            # US Passports consist of 1 letter or digit followed by 8-digits
            return self.f.bothify(text=random.choice(["?", "#"]) + "########")

        def us_drivers_license(self):
            # US driver's licenses consist of 9 digits (patterns vary by state)
            return self.f.numerify(text="### ### ###")

        def alphanum(self):
            alphanumeric_string = "".join(
                [random.choice(["?", "#"])
                 for _ in range(random.randint(1, 15))]
            )
            return self.f.bothify(text=alphanumeric_string)

        def string(self):
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
