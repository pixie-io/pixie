import {
  createStyles, makeStyles, Theme, withStyles,
} from '@material-ui/core/styles';
import ListItemIcon from '@material-ui/core/ListItemIcon';
import ListItemText from '@material-ui/core/ListItemText';
import Menu from '@material-ui/core/Menu';

export const UseKeyListStyles = makeStyles((theme: Theme) => createStyles({
  keyValue: {
    padding: 0,
    fontWeight: theme.typography.fontWeightLight,
    fontSize: '14px',
    color: theme.palette.foreground.one,
    backgroundColor: theme.palette.foreground.grey3,
    borderWidth: 8,
    borderColor: theme.palette.background.default,
  },
  actionsButton: {
    padding: 0,
  },
  copyBtn: {
    minWidth: '30px',
  },
  error: {
    padding: theme.spacing(1),
  },
}));

export const KeyListItemIcon = withStyles(() => createStyles({
  root: {
    minWidth: 30,
    marginRight: 5,
  },
}))(ListItemIcon);

export const KeyListItemText = withStyles((theme: Theme) => createStyles({
  primary: {
    fontWeight: theme.typography.fontWeightLight,
    fontSize: '14px',
    color: theme.palette.foreground.one,
  },
}))(ListItemText);

export const KeyListMenu = withStyles((theme: Theme) => createStyles({
  paper: {
    backgroundColor: theme.palette.foreground.grey3,
    borderWidth: 8,
    borderColor: theme.palette.background.default,
  },
}))(Menu);
