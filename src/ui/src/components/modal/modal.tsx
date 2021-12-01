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

import { Close as CloseIcon } from '@mui/icons-material';
import { IconButton, Modal as MUIModal } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { buildClass } from 'app/utils/build-class';

interface ModalTriggerProps {
  trigger: React.ReactNode;
  triggerClassName?: string;
  content: React.ReactNode;
  contentClassName?: string;
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  content: {
    background: theme.palette.background.default,
  },
  closeButton: {
    position: 'absolute',
    top: theme.spacing(2),
    right: theme.spacing(2),
  },
}), { name: 'Modal' });

export const ModalTrigger = React.memo<ModalTriggerProps>(({
  trigger,
  triggerClassName,
  content,
  contentClassName,
}) => {
  const [open, setOpen] = React.useState(false);
  const openModal = React.useCallback(() => setOpen(true), []);
  const closeModal = React.useCallback(() => setOpen(false), []);
  const classes = useStyles();

  return (
    <>
      <div onClick={openModal} className={buildClass(triggerClassName)}>
        {trigger}
      </div>
      <MUIModal
        open={open}
        onClose={closeModal}
        className={buildClass(classes.content, contentClassName)}
      >
        <div>
          <IconButton onClick={closeModal} className={classes.closeButton}>
            <CloseIcon />
          </IconButton>
          {content}
        </div>
      </MUIModal>
    </>
  );
});
ModalTrigger.displayName = 'ModalTrigger';
