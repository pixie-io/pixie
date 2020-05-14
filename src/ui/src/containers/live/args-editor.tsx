import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';

import { ArgsContext } from './context/args-context';

const ArgsEditor = () => {
  const { args, setArgs } = React.useContext(ArgsContext);

  if (!args) {
    return null;
  }

  const argsList = Object.entries(args).filter(([argName]) => argName !== 'script');
  return (
    <>
      {
        argsList.map(([argName, argVal]) => (
          <ArgumentField
            name={argName}
            value={argVal}
            onValueChange={(newVal) => { setArgs({ ...args, [argName]: newVal }); }}
          />
        ))
      }
    </>
  );
};

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    root: {
      marginLeft: theme.spacing(1),
      display: 'flex',
      flexDirection: 'row',
    },
    input: {
      backgroundColor: 'transparent',
      border: 'none',
      padding: 0,
      color: theme.palette.foreground.two,
      borderBottom: `1px solid ${theme.palette.foreground.one}`,
      marginLeft: theme.spacing(0.5),
      position: 'absolute',
      left: 0,
      top: 0,
      width: '100%',
      ...theme.typography.subtitle2,
      fontWeight: theme.typography.fontWeightLight,
      '&:focus': {
        borderBottom: `1px solid ${theme.palette.primary.main}`,
      },
    },
    measurer: {
      position: 'relative',
    },
    measurerContent: {
      opacity: 0,
      minWidth: theme.spacing(2),
      paddingRight: theme.spacing(1),
    },
  }),
);

interface ArgumentInputFieldProps {
  name: string;
  value: string;
  onValueChange: (val: string) => void;
}

const ArgumentField = (props: ArgumentInputFieldProps) => {
  const classes = useStyles();
  const { name, value, onValueChange } = props;
  const ref = React.useRef(null);
  return (
    <Tooltip title='Edit arg'>
      <div className={classes.root}>
        <span onClick={() => { ref.current.focus(); }}>{name}</span>:
        <div className={classes.measurer}>
          <div className={classes.measurerContent}>{value}</div>
          <input
            ref={ref}
            className={classes.input}
            value={value}
            onChange={(event) => onValueChange(event.target.value)}
          />
        </div>
      </div >
    </Tooltip >
  );
};

export default ArgsEditor;
