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
import random
import baluhn
from faker import Faker


class Providers:
    def __init__(self):
        f = Faker()
        self.f = f
        custom = self.CustomProviders(f)
        # map custom, language/region-specific pii keywords to providers
        # (labels matched with case insensitive regex)
        self.pii_label_to_provider = {
            # ------ Names ------
            "customer": f.name,
            "user": f.name,
            "full name": f.name,
            "shareholder": f.name,
            "owner": f.name,
            "first name": f.first_name,
            "given name": f.first_name,
            "last name": f.last_name,
            "family name": f.last_name,
            "company": f.company,
            "client": f.company,
            "organization": f.company,
            "dba": f.company,
            "doing business as": f.company,
            "business name": f.company,
            "business": f.company,
            # ------ Location ------
            "address": f.address,
            "home": f.address,
            "work": f.address,
            "venue": f.address,
            "place": f.address,
            "spot": f.address,
            "country": f.country,
            "destination": f.country,
            "origin": f.country,
            "nationality": f.country,
            "country code": f.country_code,
            "phone country code": f.country_code,
            "state": f.state,
            "province": f.state,
            "region": f.state,
            "federal state": f.state,
            "city": f.city,
            "bank city": f.city,
            "municipality": f.city,
            "urban area": f.city,
            "zip": f.postcode,
            "post code": f.postcode,
            "zip code": f.postcode,
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
            # ------ Financial ------
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
            "uuid": f.uuid4,
            "signature sha1": f.sha1,
            "serial": f.sha1,
            "balance": f.random_number,
            "amount": f.random_number,
            "credit": f.random_number,
            # ------ Time ------
            "date": f.date,
            "birth day": f.date,
            "birth date": f.date,
            "year": f.year,
            "birth year": f.year,
            "month": f.month,
            "birth month": f.month,
            "time stamp": f.unix_time,
            "unix time": f.unix_time,
            # ------ Identification ------
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
            # ------ Contact Info ------
            "email": f.email,
            "phone": f.phone_number,
            "phone number": f.phone_number,
            # ------ Demographic ------
            "gender": custom.gender,
            "sexuality": custom.gender,
            "sex": custom.gender,
            "occupation": f.job,
            "job": f.job,
            "profession": f.job,
            "employment": f.job,
            "vocation": f.job,
            "career": f.job,
            # ------ Internet / Devices ------
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
            "taxId": custom.alphanum,
            # ------ Misc ------
            "password": f.password,
            "file": f.file_name,
        }
        self.nonpii_label_to_provider = {
            "alphanumeric": custom.alphanum,
            "id": custom.alphanum,
            "org id": custom.alphanum,
            "statement id": custom.alphanum,
            "string": custom.string,
            "text": f.text,
            "number": f.random_number,
        }

    def get_pii(self, name):
        """Find label that matches input name (if exists), and return generated pii value using matched provider"""
        if not name:
            return (None, None)
        for label, provider in self.pii_label_to_provider.items():
            # returns generator if name is at least partial match with one of the providers
            if re.match(label, name, re.IGNORECASE):
                return (label, str(provider()))
        return (None, None)

    def get_nonpii(self, name):
        """Find label that matches input name (if exists), and return generated pii value using matched provider"""
        if not name:
            return (None, None)
        for label, provider in self.nonpii_label_to_provider.items():
            # returns generator if name is at least partial match with one of the providers
            if re.match(label, name, re.IGNORECASE):
                return (label, str(provider()))
        return (None, None)

    def get_faker(self, faker_provider):
        faker_generator = getattr(self.f, faker_provider)
        return faker_generator

    def get_random_pii(self):
        """choose random label and generate a pii value"""
        label = random.choice(list(self.pii_label_to_provider.keys()))
        return self.get_pii(label)

    def sample_pii(self, percent):
        """Sample a random percentage of pii labels and associated pii values"""
        labels = random.sample(
            list(self.pii_label_to_provider.keys()),
            round(len(self.pii_label_to_provider.keys()) * percent),
        )
        return [self.get_pii(label) for label in labels]

    class CustomProviders:
        def __init__(self, faker):
            self.f = faker

        def mac_address(self):
            pattern = random.choice([
                "^^:^^:^^:^^:^^:^^",
                "^^-^^-^^-^^-^^-^^",
                "^^ ^^ ^^ ^^ ^^ ^^",
            ])
            return self.f.hexify(pattern)

        def imei(self):
            imei = self.f.numerify(text="##-######-######-#")
            while baluhn.verify(imei.replace("-", "")) is False:
                imei = self.f.numerify(text="##-######-######-#")
            return imei

        def gender(self):
            return random.choice(["Male", "Female", "Other"])

        def us_passport(self):
            # US Passports consist of 1 letter or digit followed by 8-digits
            return self.f.bothify(text=random.choice(["?", "#"]) + "########")

        def us_drivers_license(self):
            # US driver's licenses consist of 9 digits (patterns vary by state)
            return self.f.numerify(text="### ### ###")

        def alphanum(self):
            return self.f.bothify(
                text="".join(
                    [random.choice(["?", "#"]) for _ in range(random.randint(1, 20))]
                )
            )

        def string(self):
            return self.f.lexify(
                text="".join(["?" for _ in range(random.randint(1, 20))])
            )
