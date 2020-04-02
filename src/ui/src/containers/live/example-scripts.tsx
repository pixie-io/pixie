import * as QueryString from 'query-string';
import * as React from 'react';
import {GetPxScripts, Script} from 'utils/script-bundle';

import FormControl from '@material-ui/core/FormControl';
import InputLabel from '@material-ui/core/InputLabel';
import MenuItem from '@material-ui/core/MenuItem';
import Select from '@material-ui/core/Select';
import {createStyles, makeStyles, Theme} from '@material-ui/core/styles';

import {LiveContext} from './context';

const useStyles = makeStyles((theme: Theme) =>
  createStyles({
    form: {
      margin: theme.spacing(1),
    },
    formLabel: {
      marginLeft: theme.spacing(1),
    },
  }));

interface ScriptMap {
  [title: string]: Script;
}

export const ExampleScripts = () => {
  const classes = useStyles();

  const [liveScriptMap, setLiveScriptMap] = React.useState<ScriptMap>({});
  const [liveScripts, setLiveScripts] = React.useState<Script[]>([]);

  const { executeScript } = React.useContext(LiveContext);

  React.useEffect(() => {
    GetPxScripts().then((examples) => {
      const scripts = [];
      const map = {};
      for (const s of examples) {
        if (s.vis && s.placement) {
          scripts.push(s);
          map[s.title] = s;
        }
      }
      setLiveScriptMap(map);
      setLiveScripts(scripts);
    });
  }, []);

  const { setScripts } = React.useContext(LiveContext);

  const selectScript = (e) => {
    const s = liveScriptMap[e.target.value];
    setScripts(s.code, s.vis, s.placement, { title: s.title, id: s.id });
    executeScript(s.code);
  };

  return (
    <FormControl className={classes.form}>
      <InputLabel className={classes.formLabel} id='preset-script'>Example Scripts</InputLabel>
      <Select
        labelId='preset-script'
        onChange={selectScript}
        value={''}
      >
        {
          liveScripts.map((s) => {
            return <MenuItem value={s.title} key={s.title}>{s.title}</MenuItem>;
          })
        }
      </Select>
    </FormControl>
  );
};
