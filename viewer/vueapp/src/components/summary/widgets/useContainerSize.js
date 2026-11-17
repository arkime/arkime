/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

The size of a widget's chart container, for the charts drawn at a given width
and height (combo, scatter, box plot). A WidgetCard only renders its content
once the data is in, so the container to measure (and watch) exists after each
result, not at mount: the widget calls observe() from its result handler.
*/
import { ref, nextTick, onBeforeUnmount } from 'vue';

/**
 * @returns {{ container: import('vue').Ref<HTMLElement|null>, width: import('vue').Ref<number>,
 *   height: import('vue').Ref<number>, observe: () => void }} container is the
 *   template ref of the element to measure
 */
export function useContainerSize () {
  const container = ref(null);
  const width = ref(0);
  const height = ref(0);
  let ro;

  const measure = () => {
    if (!container.value) { return; }
    width.value = container.value.clientWidth;
    height.value = container.value.clientHeight;
  };

  const observe = () => {
    nextTick(() => {
      if (!container.value) { return; }
      if (ro) { ro.disconnect(); }
      if (typeof ResizeObserver !== 'undefined') {
        ro = new ResizeObserver(measure);
        ro.observe(container.value);
      }
      measure();
    });
  };

  onBeforeUnmount(() => { if (ro) { ro.disconnect(); } });

  return { container, width, height, observe };
}
