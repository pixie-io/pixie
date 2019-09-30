import {DialogBox} from 'components/dialog-box/dialog-box';
import * as React from 'react';

export class AuthSuccess extends React.Component<{}, {}> {
    render() {
        return (
            <DialogBox width={480}>
                <div>Authentication successful. Please close this page.</div>
            </DialogBox>
        );
    }
}
