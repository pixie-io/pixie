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
  title: React.ReactNode;
  key: string;
  content: React.ReactNode;
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
        title={item.title}
        onClick={React.useCallback(() => {
          setActiveKey((key) => key === item.key ? '' : item.key);
        }, [])}
        active={activeKey === item.key}
      />);
    children.push(
      <Collapse key={`collapse-${item.key}`} in={activeKey === item.key}>
        <div className='pixie-accordion-collapse'>
          {item.content}
        </div>
      </Collapse>);
  }
  return <div className='pixie-accordion'> {children}</div>;
};

interface AccordionToggleProps {
  eventKey: string;
  active?: boolean;
  title: React.ReactNode;
  onClick?: () => void;
}

export const AccordionToggle = ({ active, title, onClick }: AccordionToggleProps) => {
  return (
    <Button className='pixie-accordion-toggle' onClick={onClick}>
      <img className='pixie-accordion-toggle-collapse-icon' src={active ? openedIcon : closedIcon} />
      {title}
    </Button>
  );
};

interface AccordionListProps {
  items: AccordionItem[];
}

interface AccordionItem {
  title: React.ReactNode;
  onClick: () => void;
}

export const AccordionList = (props: AccordionListProps) => {
  return (<>
    {
      props.items.map((item, i) => (
        <Button
          key={`accordion-child-${i}`}
          className='pixie-accordion-item'
          size='sm'
          onClick={item.onClick}
        >
          {item.title}
        </Button>
      ))
    }
  </>);
};
