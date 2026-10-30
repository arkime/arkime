<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
-->
<!--
  Wireshark-style hex pane. Renders the raw frame the tshark API tapped off the
  pcap stream, highlights the byte range of whichever dissection field is
  selected in the tree, and emits the offset of any byte the user clicks so the
  tree can jump to the field that covers it.
-->
<template>
  <div class="shark-bytes">
    <div
      v-if="!hex"
      class="text-medium-emphasis small p-2">
      No raw bytes for this packet.
    </div>
    <template v-else>
      <div
        ref="rowsRef"
        class="shark-bytes-rows"
        @click="onClick">
        <div
          v-for="row in rows"
          :key="row.offset"
          class="shark-bytes-row">
          <span class="shark-bytes-offset">{{ row.hexOffset }}</span>
          <span class="shark-bytes-hex">
            <span
              v-for="cell in row.cells"
              :key="cell.i"
              :data-off="cell.i"
              :class="{ 'shark-bytes-hl': cell.i >= hlStart && cell.i < hlEnd }">{{ cell.hex }}</span>
          </span>
          <span class="shark-bytes-ascii">
            <span
              v-for="cell in row.cells"
              :key="cell.i"
              :data-off="cell.i"
              :class="{ 'shark-bytes-hl': cell.i >= hlStart && cell.i < hlEnd }">{{ cell.ascii }}</span>
          </span>
        </div>
      </div>
      <div
        v-if="truncated"
        class="text-medium-emphasis small p-2">
        Showing the first {{ byteLength.toLocaleString() }} of {{ capturedLength.toLocaleString() }} captured bytes.
      </div>
    </template>
  </div>
</template>

<script setup>
import { computed, nextTick, ref, watch } from 'vue';

const props = defineProps({
  hex: { type: String, default: '' },
  // { pos, size } of the selected dissection field, in frame-relative bytes
  highlight: { type: Object, default: null },
  // frame.cap_len -- the real captured size, which `hex` may be truncated from
  frameLength: { type: Number, default: 0 }
});
const emit = defineEmits(['select-offset']);

const PER_ROW = 16;
const MAX_BYTES = 16384;
const rowsRef = ref(null);

// what we were sent, vs what the frame actually holds -- the API caps `hex`,
// so byteLength alone would under-report how much is missing
const byteLength = computed(() => Math.min(Math.floor((props.hex || '').length / 2), MAX_BYTES));
const capturedLength = computed(() => Math.max(props.frameLength || 0, Math.floor((props.hex || '').length / 2)));
const truncated = computed(() => capturedLength.value > byteLength.value);

const bytes = computed(() => {
  const hex = props.hex || '';
  const n = Math.min(byteLength.value, MAX_BYTES);
  const out = new Uint8Array(n);
  for (let i = 0; i < n; i++) { out[i] = parseInt(hex.substr(i * 2, 2), 16); }
  return out;
});

const hlStart = computed(() => {
  const hl = props.highlight;
  return hl && Number.isFinite(hl.pos) && hl.size > 0 ? hl.pos : -1;
});
const hlEnd = computed(() => (hlStart.value >= 0 ? hlStart.value + props.highlight.size : -1));

// Wireshark leaves a wider gutter after the 8th byte of each row.
const sepAfter = (col) => (col === 7 ? '  ' : ' ');
const asciiChar = (b) => (b >= 0x20 && b <= 0x7e ? String.fromCharCode(b) : '.');

const rows = computed(() => {
  const data = bytes.value;
  const out = [];
  for (let start = 0; start < data.length; start += PER_ROW) {
    const end = Math.min(start + PER_ROW, data.length);
    const cells = [];
    for (let i = start; i < end; i++) {
      cells.push({
        i,
        hex: data[i].toString(16).padStart(2, '0') + sepAfter(i - start),
        ascii: asciiChar(data[i])
      });
    }
    out.push({
      offset: start,
      hexOffset: start.toString(16).padStart(4, '0'),
      cells
    });
  }
  return out;
});

// One listener on the container rather than 16 per row -- the byte spans carry
// their offset in data-off.
const onClick = (e) => {
  const off = e.target?.dataset?.off;
  if (off === undefined) { return; }
  emit('select-offset', parseInt(off));
};

// keep the highlighted range on screen as the user walks the field tree
watch(() => props.highlight, (hl) => {
  if (!hl || !Number.isFinite(hl.pos)) { return; }
  nextTick(() => {
    const idx = Math.floor(hl.pos / PER_ROW);
    rowsRef.value?.children?.[idx]?.scrollIntoView({ block: 'nearest' });
  });
});
</script>

<style scoped>
.shark-bytes {
  padding: 0.25rem 0.5rem;
}
.shark-bytes-row {
  display: flex;
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 0.8rem;
  line-height: 1.35;
}
.shark-bytes-offset {
  flex: 0 0 auto;
  margin-right: 1ch;
  color: rgb(var(--v-theme-primary));
}
/* 16 hex pairs + 16 separators, one of which is doubled -- fixing the width in
   ch keeps the ascii column aligned on the short final row */
.shark-bytes-hex {
  flex: 0 0 49ch;
  white-space: pre;
}
.shark-bytes-ascii {
  flex: 0 0 auto;
  margin-left: 1ch;
  white-space: pre;
  opacity: 0.85;
}
.shark-bytes-hex span,
.shark-bytes-ascii span {
  cursor: pointer;
}
.shark-bytes-hl {
  background: rgb(var(--v-theme-primary));
  color: rgb(var(--v-theme-on-primary));
  border-radius: 2px;
}
</style>
