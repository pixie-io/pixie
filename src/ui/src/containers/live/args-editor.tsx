import * as React from 'react';

import { createStyles, makeStyles, Theme } from '@material-ui/core/styles';
import Tooltip from '@material-ui/core/Tooltip';

import { ArgsContext } from './context/args-context';
import { RouteContext } from './context/route-context';
import {ExecuteContext} from './context/execute-context';
import {ScriptContext} from './context/script-context';
import {VisContext} from './context/vis-context';

const ArgsEditor = () => {
  const { args, setArgs } = React.useContext(ArgsContext);
  const { entityParams, setEntityParams, liveViewPage } = React.useContext(RouteContext);
  const { script, title, id } = React.useContext(ScriptContext);
  const {vis} = React.useContext(VisContext);
  const { execute } = React.useContext(ExecuteContext);

  if (!args) {
    return null;
  }

  const allArgs = {
    ...args,
    ...entityParams,
  };

  const argsList = Object.entries(allArgs).filter(([argName]) => argName !== 'script');
  return (
    <>
      {
        argsList.map(([argName, argVal]: [string, string]) => (
          <ArgumentField
            key={argName}
            name={argName}
            value={argVal}
            onValueChange={(newVal) => {
              if (typeof entityParams[argName] === 'string') {
                setEntityParams({...entityParams, [argName]: newVal});
              } else {
                setArgs({ ...args, [argName]: newVal }, Object.keys(entityParams));
              }
            }}
            onEnterKey={() => {
              execute({script, vis, args, id, title, entityParams, liveViewPage});
            }}
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
  onEnterKey: () => void;
}

const ENTER_KEY_CODE = 13;

const ArgumentField = (props: ArgumentInputFieldProps) => {
  const classes = useStyles();
  const { name, value, onValueChange, onEnterKey } = props;
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
            onKeyPress={(event) => {
              if (event.which === ENTER_KEY_CODE) {
                onEnterKey();
              }
            }}
          />
        </div>
      </div >
    </Tooltip >
  );
};

export default ArgsEditor;
