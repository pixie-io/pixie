import clsx from 'clsx';
import * as React from 'react';

import IconButton from '@material-ui/core/IconButton';
import MUIModal from '@material-ui/core/Modal';
import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import CloseButton from '@material-ui/icons/Close';

interface ModalTrigerProps {
  trigger: React.ReactNode;
  triggerClassName?: string;
  content: React.ReactNode;
  contentClassName?: string;
}

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    content: {
      background: theme.palette.background.default,
    },
    closeButton: {
      position: 'absolute',
      top: theme.spacing(2),
      right: theme.spacing(2),
    },
  })
);

export const ModalTrigger: React.FC<ModalTrigerProps> = ({
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
      <div onClick={openModal} className={clsx(triggerClassName)}>
        {trigger}
      </div>
      <MUIModal
        open={open}
        onClose={closeModal}
        className={clsx(classes.content, contentClassName)}
      >
        <div>
          <IconButton onClick={closeModal} className={classes.closeButton}>
            <CloseButton />
          </IconButton>
          {content}
        </div>
      </MUIModal>
    </>
  );
};
