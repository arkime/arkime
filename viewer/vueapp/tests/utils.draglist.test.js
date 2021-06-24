import { render, fireEvent } from '@testing-library/vue';
import DragList from '../src/components/utils/DragList.vue';
import dragListComponent from './components/draglist.test.vue';

test('draggable list', async () => {
  const {
    getAllByText, getByText, queryByText
  } = render(dragListComponent, {}, (vue) => {
    vue.component('drag-list', DragList);
  });

  getByText('test0'); // has label test0

  await fireEvent.click(getAllByText('x')[0]);

  expect(queryByText('test0')).toBeNull(); // label test0 has been removed

  fireEvent.click(getByText('test1')); // clicking test1 should do nothing

  getByText('test1'); // has label test1 (not removed by remove event)

  fireEvent.drag(getByText('test1')); // drag test1 label
  fireEvent.dragOver(getByText('test2')); // over test2
  fireEvent.dragOver(getByText('test3')); // and test3
  await fireEvent.drop(getByText('test3'));

  const newOrder = ['test2', 'test3', 'test1', 'test4'];
  const labels = getAllByText(/test/i);

  // determine if the labels are in the correct order (newOrder)
  labels.forEach((label, index) => {
    expect(label.textContent.match(/test\d/i)[0]).toBe(newOrder[index]);
  });
});
