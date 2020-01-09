import './accordion.scss';

// @ts-ignore : TS does not like image files.
import * as closedIcon from 'images/icons/collapse-closed.svg';
// @ts-ignore : TS does not like image files.
import * as openedIcon from 'images/icons/collapse-opened.svg';
import * as React from 'react';
import {Button, Collapse} from 'react-bootstrap';

interface AccordionProps {
  items: AccordionToggleItem[];
}

interface AccordionToggleItem {
  name: string;
  key: string;
  children: AccordionItem[];
}

interface AccordionItem {
  name: string;
  onClick: () => void;
}

export const Accordion = (props: AccordionProps) => {
  const { items } = props;
  if (items.length < 1) {
    return null;
  }
  const defaultActiveKey = items[0].key;
  const [activeKey, setActiveKey] = React.useState(defaultActiveKey);
  const children = [];
  for (const item of items) {
    children.push(
      <AccordionToggle
        key={`toggle-${item.key}`}
        eventKey={item.key}
        name={item.name}
        onClick={React.useCallback(() => {
          setActiveKey((key) => key === item.key ? '' : item.key);
        }, [])}
        active={activeKey === item.key}
      />);
    children.push(
      <Collapse key={`collapse-${item.key}`} in={activeKey === item.key}>
        <div className='pixie-accordion-collapse'>
          {
            item.children.map((child, i) => (
              <Button
                key={`accordion-child-${i}`}
                className='pixie-accordion-item'
                size='sm'
                onClick={child.onClick}
                // @ts-ignore: 'darker' is defined in theme.scss.
                variant='darker'
              >
                {child.name}
              </Button>
            ))
          }
        </div>
      </Collapse>);
  }
  return <div className='pixie-accordion'> {children}</div>;
};

interface AccordionItemProps {
  eventKey: string;
  active?: boolean;
  name: string;
  onClick?: () => void;
}

export const AccordionToggle = ({ active, name, onClick }: AccordionItemProps) => {
  return (
    <Button className='pixie-accordion-toggle' onClick={onClick}>
      <img className='pixie-accordion-toggle-collapse-icon' src={active ? openedIcon : closedIcon} />
      {name}
    </Button>
  );
};
