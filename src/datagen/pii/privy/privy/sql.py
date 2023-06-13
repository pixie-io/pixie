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

import random

from pypika import Case, Criterion, Table, functions
from pypika.dialects import MySQLQuery, PostgreSQLQuery


class SQLQueryBuilder:
    """Build sql query from json"""

    def __init__(self, providers):
        self.payload = None
        self.query_types = {
            "select": ["orderby", "where", "limit", "groupby", "case"],
            "insert": ["columns"],
            "update": ["where"],
        }
        self.providers = providers

    def get_random_sample(self, min, source):
        """return random sample from input list. Min number of samples is 1"""
        return random.sample(list(source), random.randint(min, len(source)))

    def get_random_val(self, source):
        return random.choice(list(source))

    def _where(self, pii_labels, query):
        """Add where clause to query"""
        conditions = []
        for label in pii_labels:
            table_col = getattr(self.table, label)
            options = [
                table_col == self.payload[label],
                table_col,
                table_col != self.payload[label],
                (table_col == self.payload[label]) | (table_col == self.providers.get_faker("word")()),
            ]
            # todo: remove useful try except? What does the first cast to int do?
            try:
                int(self.payload[label])
                options.append(table_col > random.randint(0, 1000))
                options.append(table_col < random.randint(0, 1000))
                options.append(table_col <= random.randint(0, 1000))
                options.append(table_col >= random.randint(0, 1000))
            except Exception:
                pass
            conditions.append(self.get_random_val(options))
        conditions = Criterion.all(conditions)
        query = query(conditions)
        return query

    def _case(self, pii_labels, query):
        """Add case statement to select query"""
        case = Case()
        for label in pii_labels:
            table_col = getattr(self.table, label)
            options = [
                table_col == self.payload[label],
                table_col,
                table_col != self.payload[label],
                (table_col == self.payload[label]) | (table_col == self.providers.get_faker("word")()),
            ]
            try:
                int(self.payload[label])
                options.append(table_col > random.randint(0, 1000))
                options.append(table_col < random.randint(0, 1000))
                options.append(table_col <= random.randint(0, 1000))
                options.append(table_col >= random.randint(0, 1000))
                case = case.when(self.get_random_val(options), random.randint(0, 1000))
                continue
            except Exception:
                pass
            case = case.when(self.get_random_val(options), self.providers.get_faker("word")())
        return query.select(table_col, case)

    def _having(self, pii_label, query):
        """Add having clause to select query"""
        funs = [
            functions.Count,
            functions.Sum,
            functions.Avg,
            functions.Min,
            functions.Max,
            functions.Std,
            functions.StdDev,
        ]
        fun = self.get_random_val(funs)
        condition = random.randint(0, 10000)
        options = [
            fun(pii_label) >= condition,
            fun(pii_label) <= condition,
            fun(pii_label) == condition,
            fun(pii_label) != condition,
            fun(pii_label) > condition,
            fun(pii_label) < condition,
        ]
        query = query.having(self.get_random_val(options))
        return query

    def _select(self, pii_labels, pii_labels_str, query):
        """construct select query"""
        query = query.select(pii_labels_str)
        # choose subtypes (e.g. for select this could be orderby, where, limit, offset)
        subtypes = self.get_random_sample(1, self.query_types["select"])
        # self.logger.debug("subtypes", subtypes)
        for subtype in subtypes:
            # get method for this subtype e.g. select.where()
            query_method = getattr(query, subtype)
            if subtype == "orderby":
                # call function with labels e.g. select.orderby("name, address")
                query = query_method(pii_labels_str)
            elif subtype == "where":
                query = self._where(pii_labels, query_method)
            elif subtype == "limit":
                query = query_method(random.randint(0, 100))
            elif subtype == "groupby":
                query = query_method(self.get_random_val(pii_labels))
                query = self._having(self.get_random_val(pii_labels), query)
            elif subtype == "case":
                query = self._case(pii_labels, query)
        return query

    def _update(self, pii_labels, query):
        """construct update query"""
        subtypes = self.get_random_sample(0, self.query_types["update"])
        if subtypes:  # where
            query = getattr(
                query.update(self.table).set(
                    pii_labels[0], self.payload[pii_labels[0]]
                ),
                "where",
            )
            query = self._where(pii_labels, query)
        else:
            query = query.update(self.table).set(
                pii_labels[0], self.payload[pii_labels[0]]
            )
        return query

    def _insert(self, pii_vals_str, pii_labels_str, query):
        """construct insert query"""
        subtypes = self.get_random_sample(0, self.query_types["insert"])
        if subtypes:  # with columns
            query = query.into(self.table).columns(pii_labels_str).insert(pii_vals_str)
        else:  # no columns shown
            query = query.into(self.table).insert(pii_vals_str)
        return query

    def build_query(self, payload):
        """Build sql query from json"""
        self.payload = payload
        table = Table(self.providers.get_faker("word")())
        self.table = table
        self.dialects = [
            MySQLQuery.from_(table),
            PostgreSQLQuery.from_(table),
        ]
        query = random.choice(self.dialects)
        # choose a query type e.g. select
        query_type = self.get_random_val(self.query_types.keys())
        # choose selection of json keys (pii types), e.g. "name, address"
        pii_labels = self.get_random_sample(1, self.payload.keys())
        pii_labels_str = ",".join(pii_labels)
        pii_vals = [str(self.payload[pii_label]) for pii_label in pii_labels]
        pii_vals_str = ",".join(pii_vals)
        if query_type == "insert":
            query = self._insert(pii_vals_str, pii_labels_str, query)
        if query_type == "update":
            query = self._update(pii_labels, query)
        if query_type == "select":
            query = self._select(pii_labels, pii_labels_str, query)
        return query
