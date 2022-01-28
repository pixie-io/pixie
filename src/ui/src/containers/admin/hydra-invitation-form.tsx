/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as React from 'react';

import { gql, useMutation } from '@apollo/client';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { Form } from 'app/components';
import { OAUTH_PROVIDER } from 'app/containers/constants';
import { GQLUserInvite } from 'app/types/schema';

const useStyles = makeStyles((theme: Theme) => createStyles({
  invitationForm: {
    padding: theme.spacing(2),
  },
  invitationRow: {
    display: 'grid',
    gridTemplateAreas: '"name email" "link link"',
    padding: theme.spacing(2),
    marginTop: theme.spacing(4),
    backgroundColor: theme.palette.background.paper,
    border: theme.palette.border.focused,
    borderRadius: theme.shape.borderRadius,
    maxWidth: theme.breakpoints.values.md,
  },
  name: {
    fontSize: theme.typography.h4.fontSize,
  },
  email: {
    fontSize: theme.typography.caption.fontSize,
    color: theme.palette.foreground.grey1,
  },
  link: { color: theme.palette.graph.ramp[1] },
  errorRow: {
    borderRadius: theme.shape.borderRadius,
    fontWeight: theme.typography.fontWeightBold,
    backgroundColor: theme.palette.error.main,
    color: theme.palette.error.contrastText,
    maxWidth: theme.breakpoints.values.md,
    padding: theme.spacing(2),
    marginTop: theme.spacing(4),
  },
}), { name: 'HydraInvitationForm' });

// eslint-disable-next-line react-memo/require-memo
export const HydraInvitationForm: React.FC = () => {
  const classes = useStyles();

  const [inviteUser] = useMutation<
  { InviteUser: GQLUserInvite }, { email: string, firstName: string, lastName: string }
  >(gql`
    mutation InviteUser($email: String!, $firstName: String!, $lastName: String!) {
      InviteUser(email: $email, firstName: $firstName, lastName: $lastName) {
        email
        inviteLink
      }
    }
  `);

  const [fields, setFields] = React.useState<Record<string, string>>({
    'Given Name': '',
    'Family Name': '',
    'Email Address': '',
  });

  const [knownLinks, setKnownLinks] = React.useState<Array<{
    givenName: string, familyName: string, email: string, link: string
  }>>([]);
  const [errors, setErrors] = React.useState<string[]>([]);

  const update = React.useCallback((e) => {
    const { name, value } = e.currentTarget;
    fields[name] = value;
    setFields(fields);
  }, [fields, setFields]);

  if (OAUTH_PROVIDER !== 'hydra') return null;

  return (
    <div className={classes.invitationForm}>
      <Form
        submitBtnText='Invite'
        defaultSubmit={false}
        // eslint-disable-next-line react-memo/require-usememo
        onClick={() => {
          inviteUser({
            variables: {
              email: fields['Email Address'] ?? '',
              firstName: fields['Given Name'] ?? '',
              lastName: fields['Family Name'] ?? '',
            },
          }).then(({ data }) => {
            setKnownLinks([...knownLinks, {
              givenName: fields['Given Name'] ?? '',
              familyName: fields['Family Name'] ?? '',
              email: data.InviteUser.email,
              link: data.InviteUser.inviteLink,
            }]);
          }).catch((e) => {
            if (typeof e === 'object' && e?.graphQLErrors) {
              errors.push(e.graphQLErrors.map((gqlError) => gqlError.message));
            } else {
              errors.push(JSON.stringify(e));
            }
            setErrors(errors);
          });
        }}
        onChange={update}
        // eslint-disable-next-line react-memo/require-usememo
        fields={[
          {
            name: 'Given Name',
            type: 'text',
            required: true,
          },
          {
            name: 'Family Name',
            type: 'text',
            required: true,
          },
          {
            name: 'Email Address',
            type: 'text',
            required: true,
          },
        ]}
        action=''
        method='POST'
      />
      {knownLinks.map((invitation) => (
        <div key={invitation.link} className={classes.invitationRow}>
          <span className={classes.name}>
            {invitation.givenName}
            {' '}
            {invitation.familyName}
          </span>
          <a href={`mailto:${invitation.email}`} className={classes.email}>{`<${invitation.email}>`}</a>
          <span className={classes.link}>{invitation.link}</span>
        </div>
      ))}
      {errors.map((error) => <div key={error} className={classes.errorRow}>{error}</div>)}
    </div>
  );
};
HydraInvitationForm.displayName = 'HydraInvitationForm';
