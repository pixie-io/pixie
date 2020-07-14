import * as React from 'react';
import {
  createStyles, Theme, WithStyles, withStyles,
} from '@material-ui/core/styles';
import ArrowDropDownIcon from '@material-ui/icons/ArrowDropDown';
import TextField from '@material-ui/core/TextField';
import Menu from '@material-ui/core/Menu';
import MenuItem from '@material-ui/core/MenuItem';
import ListItemText from '@material-ui/core/ListItemText';

const styles = ({ spacing, typography, palette }: Theme) => createStyles({
  breadcrumbs: {
    display: 'inline-flex',
  },
  breadcrumb: {
    display: 'inline-flex',
    paddingRight: spacing(0.5),
  },
  title: {
    fontWeight: typography.fontWeightBold,
    paddingRight: spacing(0.5),
    paddingLeft: spacing(0.5),
  },
  value: {
    color: palette.primary.main,
  },
  body: {
    ...typography.body2,
    backgroundColor: palette.foreground.grey3,
    display: 'inline-block',
    color: palette.text.primary,
    height: spacing(3),
  },
  content: {
    display: 'flex',
    alignItems: 'center',
    paddingLeft: spacing(1),
    height: spacing(3),
  },
  dropdownArrow: {
    cursor: 'pointer',
    height: spacing(3),
  },
  spacer: {
    width: spacing(2),
  },
  input: {
    padding: `0 ${spacing(1)}px ${spacing(1)}px ${spacing(1)}px`,
  },
  // CSS for the front/back of the breadcrumb element.
  triangle: {
    position: 'absolute',
    height: spacing(3),
    width: spacing(3),
    overflow: 'hidden',
  },
  angle: {
    float: 'right', // Take the triangle out of the DOM, so it can overlap with other breadcrumbs.
  },
  tail: {
    width: spacing(2.4),
    height: spacing(3),
  },
  topArrow: {
    position: 'absolute',
    width: spacing(3),
    height: spacing(3),
    top: -1 * spacing(1.5),
    right: -1 * spacing(1.6),
    background: palette.foreground.grey3,
    transform: 'rotate(45deg)',
    borderRadius: spacing(0.4),
  },
  bottomArrow: {
    position: 'absolute',
    width: spacing(3),
    height: spacing(3),
    bottom: `-${spacing(1.5)}px`,
    right: `-${spacing(1.6)}px`,
    background: palette.foreground.grey3,
    transform: 'rotate(45deg)',
    borderRadius: spacing(0.4),
  },
  arrow: {
    position: 'absolute',
    width: spacing(3),
    height: spacing(3),
    left: `-${spacing(1.5)}px`,
    background: palette.foreground.grey3,
    transform: 'rotate(45deg)',
    borderRadius: spacing(0.4),
  },
  inputItem: {
    '&:hover': {
      backgroundColor: 'initial',
    },
    '&.Mui-focusVisible': {
      backgroundColor: 'initial',
    },
  },
  separator: {
    display: 'flex',
    alignItems: 'center',
    color: palette.foreground.one,
    fontWeight: 1000,
    width: spacing(1),
  },
});

interface DialogDropdownProps extends WithStyles<typeof styles> {
  onSelect: (input: string) => void;
  getListItems: (input: string) => Promise<Array<string>>;
  onClose: () => void;
  showInputField: boolean;
  anchorEl: HTMLElement;
}

const DialogDropdown = ({
  classes, onSelect, onClose, getListItems, showInputfield, anchorEl,
}) => {
  const [listItems, setListItems] = React.useState<Array<string>>([]);
  const inputRef = React.useRef(null);

  React.useEffect(() => {
    if (getListItems) {
      getListItems('').then((items) => {
        setListItems(items);
      });
    }
  }, [getListItems]);

  if (anchorEl) {
    if (inputRef.current) {
      setTimeout(() => {
        inputRef.current.focus();
      }, 100);
    }
  }

  const handleClose = React.useCallback(() => {
    if (inputRef.current) {
      inputRef.current.value = '';
    }
    onClose();
  }, [inputRef, onClose]);

  const handleInput = React.useCallback((input) => {
    if (getListItems) {
      getListItems(input).then((items) => {
        setListItems(items);
      });
    }
  }, [getListItems]);

  const handleKey = React.useCallback((event) => {
    if (event.key === 'Enter') {
      onSelect(inputRef.current.value);
      handleClose();
    }
  }, [inputRef, onSelect, handleClose]);

  return (
    <Menu
      anchorEl={anchorEl}
      keepMounted
      open={Boolean(anchorEl)}
      onClose={handleClose}
      getContentAnchorEl={null}
      anchorOrigin={{
        vertical: 'bottom',
        horizontal: 'center',
      }}
      transformOrigin={{
        vertical: 'top',
        horizontal: 'center',
      }}
      elevation={0}
    >
      {
        showInputfield
        && (
          <MenuItem
            onKeyDown={(e) => {
              e.stopPropagation();
            }}
            className={classes.inputItem}
          >
            <TextField
              autoFocus
              className={classes.input}
              defaultValue=''
              size='small'
              onChange={handleInput}
              onKeyPress={handleKey}
              inputRef={inputRef}
            />
          </MenuItem>
        )
      }
      {
        listItems.map((item) => (
          <MenuItem
            key={item}
            onClick={() => {
              onSelect(item);
              handleClose();
            }}
          >
            <ListItemText primary={item} />
          </MenuItem>
        ))
      }
    </Menu>
  );
};

interface BreadcrumbProps extends WithStyles<typeof styles> {
  title: string;
  value: string;
  selectable: boolean;
  allowTyping?: boolean;
  getListItems?: (input: string) => Promise<Array<string>>;
  first: boolean;
  onSelect?: (input: string) => void;
}

const Breadcrumb = ({
  classes, title, value, selectable, allowTyping, getListItems, first, onSelect,
}: BreadcrumbProps) => {
  const [anchorEl, setAnchorEl] = React.useState<null | HTMLElement>(null);

  const handleClick = React.useCallback((event: React.MouseEvent<HTMLElement>) => {
    setAnchorEl(event.currentTarget.parentElement);
  }, [setAnchorEl]);

  const onClose = React.useCallback(() => {
    setAnchorEl(null);
  }, [setAnchorEl]);

  return (
    <div className={classes.breadcrumb}>
      <div className={classes.body}>
        <div className={classes.content}>
          <span className={classes.title}>
            {`${title}: `}
          </span>
          <span className={classes.value}>{value}</span>
          { selectable && <div className={classes.dropdownArrow} onClick={handleClick}><ArrowDropDownIcon /></div> }
          { !selectable && <div className={classes.spacer} />}
          <DialogDropdown
            classes={classes}
            onSelect={onSelect}
            onClose={onClose}
            getListItems={getListItems}
            showInputfield={allowTyping}
            anchorEl={anchorEl}
          />
        </div>
      </div>
    </div>
  );
};

interface BreadcrumbOptions {
  title: string;
  value: string;
  selectable: boolean;
  allowTyping?: boolean;
  getListItems?: (input: string) => Promise<Array<string>>;
  onSelect?: (input: string) => void;
}

interface BreadcrumbsProps extends WithStyles<typeof styles> {
  breadcrumbs: Array<BreadcrumbOptions>;
}

const Breadcrumbs = ({
  classes, breadcrumbs,
}: BreadcrumbsProps) => (
  <div className={classes.breadcrumbs}>
    {
      breadcrumbs.map((breadcrumb, i) => (
        <>
          <Breadcrumb
            key={i}
            first={i === 0}
            classes={classes}
            {...breadcrumb}
          />
          {(i !== breadcrumbs.length - 1)
            && <div className={classes.separator}>/</div> }
        </>
      ))
    }
  </div>
);

export default withStyles(styles)(Breadcrumbs);
