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
import { OAUTH_PROVIDER } from 'containers/constants';
import { Form, FormField } from '@pixie-labs/components';
import { useInvitation } from '@pixie-labs/api-react';
import { makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';

const useStyles = makeStyles((theme: Theme) => createStyles({
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
  link: { color: theme.palette.info.main },
  errorRow: {
    borderRadius: theme.shape.borderRadius,
    fontWeight: theme.typography.fontWeightBold,
    backgroundColor: theme.palette.error.main,
    color: theme.palette.error.contrastText,
    maxWidth: theme.breakpoints.values.md,
    padding: theme.spacing(2),
    marginTop: theme.spacing(4),
  },
}));

export const HydraInvitationForm: React.FC = () => {
  const classes = useStyles();

  const [fields, setFields] = React.useState<FormField[]>([
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
  ]);

  const requestInvitation = useInvitation();

  const [knownLinks, setKnownLinks] = React.useState<Array<{
    givenName: string, familyName: string, email: string, link: string
  }>>([]);
  const [errors, setErrors] = React.useState<string[]>([]);

  const update = React.useCallback((e) => {
    const { name, value } = e.currentTarget;
    const which = fields.findIndex((f) => f.name === name);
    fields[which].value = value;
    setFields(fields);
  }, [fields, setFields]);

  const submitInvitation = React.useCallback(() => {
    const records: Record<string, string> = fields.reduce((a, c) => ({ ...a, [c.name]: c.value }), {});
    const givenName = records['Given Name'];
    const familyName = records['Family Name'];
    const email = records['Email Address'];
    requestInvitation(givenName, familyName, email).then((invite) => {
      setKnownLinks([...knownLinks, {
        givenName, familyName, email: invite.email, link: invite.inviteLink,
      }]);
    }).catch((e) => {
      if (typeof e === 'object' && e?.graphQLErrors) {
        errors.push(e.graphQLErrors.map((gqlError) => gqlError.message));
      } else {
        errors.push(JSON.stringify(e));
      }
      setErrors(errors);
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  if (OAUTH_PROVIDER !== 'hydra') return null;

  return (
    <>
      <Form
        submitBtnText='Invite'
        defaultSubmit={false}
        onClick={submitInvitation}
        onChange={update}
        fields={fields}
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
    </>
  );
};
