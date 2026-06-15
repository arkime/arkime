/**
 * Copyright Yahoo Inc.
 * SPDX-License-Identifier: Apache-2.0
 **/

// Drag-to-resize mechanics for table columns and definition-list dts.
// Owns Pointer Events + min/max clamping + NaN-safe start-width capture.
// Callers stay in charge of layout consequences (table width, persistence,
// sibling alignment) via the `onCommit` callback.
//
// Usage:
//   const { startResize } = useColumnResize({ minWidth: 70 });
//   grip.addEventListener('pointerdown', (e) => {
//     startResize(e, columnEl, grip, (newWidth, delta) => {
//       // update model, table width, persist
//     });
//   });

export function useColumnResize ({
  minWidth = 70,
  getMaxWidth = () => Math.max(minWidth, window.innerWidth - 100),
  liveGuide = true,
  guideSide = 'left' // 'left' (default, for <th> grips) or 'right' (session-detail dt grips)
} = {}) {
  let col;
  let grip;
  let startW;       // requested width (style.width) — basis for commit math
  let visualStartW; // rendered width (offsetWidth) — basis for guide position
  let startX;
  let commit;

  function clamp (candidate) {
    return Math.min(getMaxWidth(), Math.max(minWidth, candidate));
  }

  function applyGuide (w) {
    if (!liveGuide || !grip) return;
    const borderProp = guideSide === 'right' ? 'borderRight' : 'borderLeft';
    grip.style[borderProp] = '1px dotted rgb(var(--v-theme-neutral))';
    grip.style.left = `${w}px`;
  }

  function clearGuide () {
    if (!grip) return;
    grip.style.borderLeft = '';
    grip.style.borderRight = '';
    grip.style.left = '';
  }

  function onPointerMove (e) {
    if (!col) return;
    // Guide tracks the cursor (offsetWidth-based) so it doesn't drift
    // when style.width and offsetWidth disagree under table-layout:auto.
    applyGuide(clamp(visualStartW + (e.clientX - startX)));
  }

  function onPointerEnd (e) {
    if (!col) {
      detach();
      return;
    }
    // Commit math uses startW (style.width) so a 50px drag = 50px column growth.
    const w = clamp(startW + (e.clientX - startX));
    const cb = commit;
    const delta = w - startW;
    clearGuide();
    col.classList.remove('col-resizing');
    col = null;
    grip = null;
    commit = null;
    detach();
    cb?.(w, delta);
  }

  function detach () {
    document.removeEventListener('pointermove', onPointerMove);
    document.removeEventListener('pointerup', onPointerEnd);
    document.removeEventListener('pointercancel', onPointerEnd);
  }

  function startResize (e, columnEl, gripEl, onCommit) {
    e.preventDefault();
    e.stopPropagation();
    col = columnEl;
    col.classList.add('col-resizing'); // style hook for the active column
    grip = gripEl;
    startW = parseInt(columnEl.style.width, 10) ||
             columnEl.offsetWidth ||
             minWidth;
    visualStartW = columnEl.offsetWidth || startW;
    startX = e.clientX;
    commit = onCommit;
    document.addEventListener('pointermove', onPointerMove);
    document.addEventListener('pointerup', onPointerEnd);
    document.addEventListener('pointercancel', onPointerEnd);
  }

  return { startResize };
}

// Higher-level helper: wires pointerdown handlers on every grip inside `cols`,
// captures before-state, applies the new width to the column, and (by default)
// extends the table width by the same delta. Callers supply `onCommit` for
// any extra state sync (header widths, persistence, mapHeaders, etc).
//
// Returns an object with `detach()` for cleanup on re-init or unmount.
//
// onCommit signature: ({ colEl, colIndex, cols, newWidth, delta, tableWidthBefore, newTableWidth }) => void
// shouldUpdateTable: optional predicate (ctx) => boolean. Default: always update.
export function attachTableGrips ({
  cols,
  table,
  minWidth = 70,
  guideSide = 'left',
  gripClassName = 'grip',
  onCommit,
  onResetAll,
  shouldUpdateTable
}) {
  const resizer = useColumnResize({ minWidth, guideSide });
  const handlers = [];

  for (let i = 0; i < cols.length; i++) {
    const colEl = cols[i];
    const grip = colEl.getElementsByClassName(gripClassName)[0];
    if (!grip) continue;
    const colIndex = i;
    const handler = (e) => {
      // Ctrl+Shift+click on any grip resets all column widths to defaults.
      // Useful for testing the in-code defaults without clearing user state
      // through the menu.
      if (onResetAll && e.ctrlKey && e.shiftKey) {
        e.preventDefault();
        e.stopPropagation();
        onResetAll();
        return;
      }
      const tableWidthBefore = parseInt(table.style.width, 10) || table.offsetWidth || 0;
      // Use the column's *requested* width (style.width) rather than
      // offsetWidth so the table-width delta matches the column-width
      // delta exactly. With table-layout:auto the browser may render
      // a column wider/narrower than requested; treating offsetWidth
      // as the baseline causes a mismatch that other columns absorb.
      const colWidthBefore = parseInt(colEl.style.width, 10) ||
                             colEl.offsetWidth || 0;
      resizer.startResize(e, colEl, grip, (newWidth) => {
        colEl.style.width = `${newWidth}px`;
        const delta = newWidth - colWidthBefore;
        const newTableWidth = tableWidthBefore + delta;
        const ctx = { colEl, colIndex, cols, newWidth, delta, tableWidthBefore, newTableWidth };
        onCommit?.(ctx);
        if (!shouldUpdateTable || shouldUpdateTable(ctx)) {
          table.style.width = `${newTableWidth}px`;
        }
      });
    };
    grip.addEventListener('pointerdown', handler);
    handlers.push({ grip, handler });
  }

  return {
    detach () {
      for (const { grip, handler } of handlers) {
        grip.removeEventListener('pointerdown', handler);
      }
      handlers.length = 0;
    }
  };
}
