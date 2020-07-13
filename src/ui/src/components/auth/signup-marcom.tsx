import * as React from 'react';
import {
  Grid, WithStyles, withStyles, createStyles, Theme, Typography, Container, fade,
} from '@material-ui/core';
import * as rustSVG from '../../../assets/images/icons/rust.svg';
import * as cppSVG from '../../../assets/images/icons/cpp.svg';
import * as golangSVG from '../../../assets/images/icons/golang.svg';
import * as heartSVG from '../../../assets/images/icons/heart.svg';
import PixieLogo from '../icons/pixie-logo';

const styles = ({ palette, spacing }: Theme) => createStyles({
  root: {
    backgroundColor: palette.background.default,
  },
  heading: {
    color: palette.foreground.white,
    textAlign: 'center',
  },
  message: {
    color: palette.primary.light,
    textAlign: 'center',
  },
  pixieLove: {
    display: 'flex',
    marginTop: spacing(5),
    alignItems: 'center',
    background: `linear-gradient(180deg, ${fade(palette.background.two, 0.87)},
    ${fade(palette.background.three, 0.22)})`,
    boxShadow: `2px 2px 2px 0px ${palette.background.default}`,
    paddingLeft: spacing(2),
    paddingRight: spacing(2),
  },
  pixieLoveItem: {
    display: 'flex',
    paddingLeft: spacing(1),
    paddingRight: spacing(1),
  },
});

export const SignupMarcom = withStyles(styles)(({ classes }: WithStyles<typeof styles>) => (
  <>
    <Container maxWidth='sm' className={classes.root}>
      <Grid
        container
        direction='column'
        spacing={4}
        justify='flex-start'
        alignItems='center'
      >
        <Grid item>
          <Typography variant='h4' className={classes.heading}>
            Instantly troubleshoot your applications on Kubernetes
          </Typography>
        </Grid>
        <Grid item>
          <Typography variant='subtitle1' className={classes.message}>
            NO CODE CHANGES. NO MANUAL INTERFACES. ALL INSIDE K8S.
          </Typography>
        </Grid>
        <Grid item>
          <div className={classes.pixieLove}>
            <PixieLogo fontSize='large' />
            <div className={classes.pixieLoveItem}>
              <img src={heartSVG} alt='Loves' />
            </div>
            <div className={classes.pixieLoveItem}>
              <img src={golangSVG} alt='Golang' />
            </div>
            <div className={classes.pixieLoveItem}>
              <img src={cppSVG} alt='C++' />
            </div>
            <div className={classes.pixieLoveItem}>
              <img src={rustSVG} alt='Rust' />
            </div>
          </div>
        </Grid>
      </Grid>
    </Container>
  </>
));
