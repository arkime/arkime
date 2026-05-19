<template>
  <div
    :ref="session.id"
    :id="`${session.id}-detail`"
    :class="['session-detail-wrapper', `card-columns-${numCols}`]">
    <!-- detail error -->
    <h5
      v-if="error"
      class="text-danger mt-3 mb-3 ms-2">
      <v-icon
        icon="mdi-alert"
        class="me-2" />
      {{ error }}
    </h5> <!-- /detail error -->

    <!-- async detail content -->
    <SessionDetailDataComponent
      :key="componentKey"
      @reload="reload"
      @toggle-col-vis="toggleColVis"
      @toggle-info-vis="toggleInfoVis" /> <!-- /async detail content -->

    <!-- packet options -->
    <div
      v-show="!hidePackets && !user.hidePcap"
      class="packet-options me-1 ms-1 mt-3">
      <form class="d-flex mb-2 pt-2 border-top">
        <fieldset :disabled="hidePackets || loadingPackets || renderingPackets">
          <packet-options
            :params="params"
            :decodings="decodings"
            :cyber-chef-src-url="cyberChefSrcUrl"
            :cyber-chef-dst-url="cyberChefDstUrl"
            @update-base="updateBase"
            @toggle-images="toggleImages"
            @toggle-show-src="toggleShowSrc"
            @toggle-show-dst="toggleShowDst"
            @apply-decodings="applyDecodings"
            @update-decodings="updateDecodings"
            @toggle-timestamps="toggleTimestamps"
            @toggle-show-frames="toggleShowFrames"
            @update-num-packets="updateNumPackets"
            @toggle-line-numbers="toggleLineNumbers"
            @toggle-compression="toggleCompression" />
        </fieldset>
      </form>
    </div> <!-- /packet options -->

    <!-- packets loading -->
    <div
      v-if="loadingPackets && !hidePackets && !user.hidePcap"
      class="mt-4 mb-4 ms-2 me-2 large">
      <v-icon
        icon="mdi-loading"
        class="fa-spin" />&nbsp;
      {{ $t('sessions.detail.loadingSessionPackets') }}&nbsp;
      <v-btn
        color="warning"
        variant="flat"
        size="x-small"
        density="comfortable"
        @click="cancelPacketLoad">
        <v-icon
          icon="mdi-cancel"
          class="me-1" />
        {{ $t('common.cancel') }}
      </v-btn>
    </div> <!-- /packets loading -->

    <!-- packets rendering -->
    <div
      v-if="renderingPackets && !hidePackets && !user.hidePcap"
      class="mt-4 mb-4 ms-2 me-2 large">
      <v-icon
        icon="mdi-loading"
        class="fa-spin" />&nbsp;
      {{ $t('sessions.detail.renderingSessionPackets') }}&nbsp;
    </div> <!-- /packets rendering -->

    <!-- packets error -->
    <div
      v-if="!error && errorPackets"
      class="mt-4 mb-4 ms-2 me-2 large">
      <span class="text-danger">
        <v-icon icon="mdi-alert" />&nbsp;
        {{ errorPackets }}&nbsp;
      </span>
      <v-btn
        color="success"
        variant="flat"
        size="x-small"
        density="comfortable"
        @click="getPackets">
        <v-icon
          icon="mdi-refresh"
          class="me-1" />
        retry
      </v-btn>
    </div> <!-- /packets error -->

    <!-- packets -->
    <div
      v-if="!loadingPackets && !errorPackets && !hidePackets && !user.hidePcap"
      class="inner packet-container me-1 ms-1"
      v-html="packetHtml"
      ref="packetContainerRef"
      :class="{'show-ts':params.ts,'hide-src':!params.showSrc,'hide-dst':!params.showDst}" /> <!-- packets -->

    <!-- tshark -->
    <div
      v-if="hasTshark && !hidePackets && !user.hidePcap"
      class="tshark-section me-1 ms-1 mt-3 pt-2 border-top">
      <div class="d-flex align-items-center mb-2">
        <strong class="me-2">tshark</strong>
        <v-btn
          v-if="!tsharkLoaded && !tsharkLoading"
          color="primary"
          variant="flat"
          size="x-small"
          density="comfortable"
          @click="getTshark">
          <v-icon
            icon="mdi-play"
            class="me-1" />
          run
        </v-btn>
        <v-btn
          v-if="tsharkLoaded && !tsharkLoading"
          color="primary"
          variant="flat"
          size="x-small"
          density="comfortable"
          class="me-2"
          @click="getTshark">
          <v-icon
            icon="mdi-refresh"
            class="me-1" />
          reload
        </v-btn>
        <span v-if="tsharkLoading">
          <v-icon
            icon="mdi-loading"
            class="fa-spin" /> running tshark…
        </span>
        <label class="ms-3 d-inline-flex align-center small">
          <input
            type="checkbox"
            class="arkime-check-input me-1"
            v-model="tsharkPayload">payload
        </label>
        <label class="ms-2 d-inline-flex align-center small">
          <input
            type="checkbox"
            class="arkime-check-input me-1"
            v-model="tsharkHidden">hidden fields
        </label>
        <label class="ms-2 d-inline-flex align-center small">
          packets
          <input
            type="number"
            class="arkime-input-control ms-1 tshark-length"
            min="1"
            v-model.number="tsharkLength">
        </label>
      </div>
      <div
        v-if="tsharkError"
        class="text-danger small mb-2">
        <v-icon
          icon="mdi-alert"
          class="me-1" />{{ tsharkError }}
      </div>
      <div
        v-if="tsharkPackets.length"
        class="tshark-output">
        <details
          v-for="(pkt, pi) in tsharkPackets"
          :key="pi"
          class="tshark-packet"
          :style="packetProtoStyle(packetTopProto(pkt))">
          <summary>
            <strong>Packet {{ pi + 1 }}</strong>
            <span
              v-if="packetTopProto(pkt)"
              class="tshark-proto-badge ms-2">{{ packetTopProto(pkt).toUpperCase() }}</span>
            <span class="ms-2 small">{{ packetSummary(pkt) }}</span>
          </summary>
          <ul class="tshark-tree">
            <tshark-node
              v-for="(layer, li) in pkt.layers"
              :key="li"
              :node="layer" />
          </ul>
        </details>
      </div>
    </div> <!-- /tshark -->

    <!-- packet options -->
    <div
      v-show="!hidePackets && !user.hidePcap"
      class="packet-options me-1 ms-1">
      <form class="d-flex mb-2 pt-2 border-top">
        <fieldset :disabled="hidePackets || loadingPackets || renderingPackets">
          <packet-options
            :params="params"
            :decodings="decodings"
            :cyber-chef-src-url="cyberChefSrcUrl"
            :cyber-chef-dst-url="cyberChefDstUrl"
            @update-base="updateBase"
            @toggle-images="toggleImages"
            @toggle-show-src="toggleShowSrc"
            @toggle-show-dst="toggleShowDst"
            @apply-decodings="applyDecodings"
            @update-decodings="updateDecodings"
            @toggle-timestamps="toggleTimestamps"
            @toggle-show-frames="toggleShowFrames"
            @update-num-packets="updateNumPackets"
            @toggle-line-numbers="toggleLineNumbers"
            @toggle-compression="toggleCompression" />
        </fieldset>
      </form>
    </div> <!-- /packet options -->
  </div>
</template>

<script setup>
// external imports
import { ref, defineAsyncComponent, computed, onMounted, nextTick, onUnmounted, inject } from 'vue';
// internal imports
import store from '@/store';
import { timezoneDateString } from '@common/vueFilters.js';
import PacketOptions from './PacketOptions.vue';
import SessionsService from './SessionsService';
import sessionDetailData from './sessionDetailData.js';
import TsharkNode from './TsharkNode.vue';
// asynchronous component defined above with html injected by createDetailDataComponent
let SessionDetailDataComponent = null;

import { useI18n } from 'vue-i18n';
const { t } = useI18n();

const defaultUserSettings = {
  detailFormat: 'last',
  numPackets: 'last',
  showTimestamps: 'last'
};

// emits
const emit = defineEmits(['toggleColVis', 'toggleInfoVis']);

// variables
const error = ref('');
const componentKey = ref(0);
const numCols = computed(() => {
  return store.state.sessionDetailCols || '';
});
const user = computed(() => {
  return store.state.user;
});
const packetContainerRef = ref(null);
const renderingPackets = ref(false);
const packetHtml = ref('');
const hidePackets = ref(false);
const errorPackets = ref('');
const loadingPackets = ref(false);
const packetPromise = ref();
const decodings = ref({});
const constants = inject('constants', {});
const hasTshark = computed(() => !!constants.HASTSHARK);
const tsharkLoading = ref(false);
const tsharkLoaded = ref(false);
const tsharkError = ref('');
const tsharkPackets = ref([]);
const tsharkLength = ref(50);
const tsharkPayload = ref(false);
const tsharkHidden = ref(false);
const tsharkPromise = ref();
const params = ref({
  base: 'natural',
  line: false,
  image: false,
  gzip: false,
  ts: false,
  decode: {},
  packets: 200,
  showFrames: false,
  showSrc: true,
  showDst: true
});
const cyberChefSrcUrl = computed(() => {
  return `cyberchef.html?nodeId=${props.session.node}&sessionId=${props.session.id}&type=src`;
});
const cyberChefDstUrl = computed(() => {
  return `cyberchef.html?nodeId=${props.session.node}&sessionId=${props.session.id}&type=dst`;
});

// props
const props = defineProps({
  session: {
    type: Object,
    required: true
  }
});

// methods
// fetch and render the session detail data
// this is an async component that will be injected into the template
const createDetailDataComponent = () => {
  return defineAsyncComponent(async () => {
    try {
      const response = await SessionsService.getDetail(props.session.id, props.session.node, props.session.cluster);
      hidePackets.value = /hidepackets="true"/i.test(response);
      return sessionDetailData.getVueInstance(response, props.session); // render the session detail data
    } catch (err) {
      console.log('Error loading session detail data', err);
      error.value = t('sessions.detail.loadingErr');
    }
  });
};
SessionDetailDataComponent = createDetailDataComponent();

const reload = async () => {
  error.value = '';
  SessionDetailDataComponent = createDetailDataComponent();
  componentKey.value++; // force re-render
};

const cancelPacketLoad = () => {
  if (packetPromise.value && packetPromise.value.controller) {
    packetPromise.value.controller.abort(t('common.youCancelledRequest'));
  }
  packetPromise.value = undefined;
  loadingPackets.value = false;
  errorPackets.value = 'Request aborted';
};

const toggleColVis = (col) => {
  emit('toggleColVis', col);
};

const toggleInfoVis = (info) => {
  emit('toggleInfoVis', info);
};

const updateBase = (value) => {
  params.value.base = value;
  getPackets();
};

const updateNumPackets = (value) => {
  params.value.packets = value;
  getPackets();
};

const toggleShowFrames = () => {
  params.value.showFrames = !params.value.showFrames;
  if (localStorage) {
    // update browser saved ts if the user settings is set to last
    localStorage['moloch-showFrames'] = params.value.showFrames;
  }

  if (params.value.showFrames) {
    // show timestamps and info by default for show frames option
    params.value.ts = true;
    // disable other options that don't make sense for raw packets
    params.value.gzip = false;
    params.value.image = false;
    params.value.decode = {};
  } else {
    // reset showFrames/gzip/image/decode options back to what was last used
    setBrowserParams();
  }

  getPackets();
};

const toggleShowSrc = () => {
  params.value.showSrc = !params.value.showSrc;
};

const toggleShowDst = () => {
  params.value.showDst = !params.value.showDst;
};

const toggleLineNumbers = () => {
  // can only have line numbers in hex mode
  if (params.value.base !== 'hex') { return; }
  params.value.line = !params.value.line;
  getPackets();
};

const toggleCompression = () => {
  params.value.gzip = !params.value.gzip;
  getPackets();
};

const toggleImages = () => {
  params.value.image = !params.value.image;
  getPackets();
};

const toggleTimestamps = () => {
  params.value.ts = !params.value.ts;
  if (localStorage && user.value.settings.showTimestamps === 'last') {
    // update browser saved ts if the user settings is set to last
    localStorage['moloch-ts'] = params.value.ts;
  }
};

const applyDecodings = (newDecodings) => {
  params.value.decode = newDecodings;
  getPackets();
};

const updateDecodings = (newDecodings) => {
  decodings.value = newDecodings;
};

const showSrcBytesImg = () => {
  const url = `api/session/raw/${props.session.node}/${props.session.id}.png?type=src`;
  packetContainerRef.value.getElementsByClassName('src-col-tip')[0].innerHTML = `Source Bytes:
    <br>
    <img src="${url}">
    <a class="no-decoration download-bytes" href="${url}" download="${props.session.id}-src.png">
      <i class="mdi mdi-download"></i>&nbsp;
      Download source bytes image
    </a>
  `;
  packetContainerRef.value.getElementsByClassName('srccol')[0].removeEventListener('mouseenter', showSrcBytesImg);
};

const showDstBytesImg = () => {
  const url = `api/session/raw/${props.session.node}/${props.session.id}.png?type=dst`;
  packetContainerRef.value.getElementsByClassName('dst-col-tip')[0].innerHTML = `Destination Bytes:
    <br>
    <img src="${url}">
    <a class="no-decoration download-bytes" href="${url}" download="${props.session.id}-dst.png">
      <i class="mdi mdi-download"></i>&nbsp;
      Download destination bytes image
    </a>
  `;
  packetContainerRef.value.getElementsByClassName('dstcol')[0].removeEventListener('mouseenter', showDstBytesImg);
};

// helpers
const setUserParams = () => {
  if (localStorage && user.value.settings) { // display user saved options
    if (user.value.settings.detailFormat === 'last' && localStorage['moloch-base'] && localStorage['moloch-base'] !== 'last') {
      params.value.base = localStorage['moloch-base'];
    } else if (user.value.settings.detailFormat && user.value.settings.detailFormat !== 'last') {
      params.value.base = user.value.settings.detailFormat;
    }

    if (user.value.settings.numPackets === 'last') {
      params.value.packets = localStorage['moloch-packets'] || 200;
    } else {
      params.value.packets = user.value.settings.numPackets || 200;
    }

    if (user.value.settings.showTimestamps === 'last' && localStorage['moloch-ts']) {
      params.value.ts = localStorage['moloch-ts'] === 'true';
    } else if (user.value.settings.showTimestamps) {
      params.value.ts = user.value.settings.showTimestamps === 'on';
    }
  } else if (!user.value.settings) {
    user.value.settings = defaultUserSettings;
  }
};

const setBrowserParams = () => {
  if (!localStorage) { return; }
  // display browser saved options
  if (localStorage['moloch-line']) {
    params.value.line = JSON.parse(localStorage['moloch-line']);
  }
  if (localStorage['moloch-gzip']) {
    params.value.gzip = JSON.parse(localStorage['moloch-gzip']);
  }
  if (localStorage['moloch-image']) {
    params.value.image = JSON.parse(localStorage['moloch-image']);
  }
  if (localStorage['moloch-showFrames']) {
    params.value.showFrames = JSON.parse(localStorage['moloch-showFrames']);
  }
  if (localStorage['moloch-decodings']) {
    params.value.decode = JSON.parse(localStorage['moloch-decodings']);
    for (const key in decodings.value) {
      decodings.value[key].active = false;
      if (params.value.decode[key]) {
        decodings.value[key].active = true;
        for (const field in params.value.decode[key]) {
          for (let i = 0, len = decodings.value[key].fields.length; i < len; ++i) {
            if (decodings.value[key].fields[i].key === field) {
              decodings.value[key].fields[i].value = params.value.decode[key][field];
            }
          }
        }
      }
    }
  }
};

const getPackets = async () => {
  // if the user is not allowed to view packets, don't request them
  if (user.value.hidePcap) { return; }

  // already loading, don't load again!
  if (loadingPackets.value || hidePackets.value) { return; }

  loadingPackets.value = true;
  errorPackets.value = false;

  if (localStorage) { // update browser saved options
    if (user.value.settings.detailFormat === 'last') {
      localStorage['moloch-base'] = params.value.base;
    }
    if (user.value.settings.numPackets === 'last') {
      localStorage['moloch-packets'] = params.value.packets || 200;
    }
    localStorage['moloch-line'] = params.value.line;
    localStorage['moloch-gzip'] = params.value.gzip;
    localStorage['moloch-image'] = params.value.image;
  }

  try {
    const { controller, fetcher } = SessionsService.getPackets(
      props.session.id,
      props.session.node,
      props.session.cluster,
      params.value
    );
    packetPromise.value = { controller };

    const response = await fetcher; // do the fetch

    loadingPackets.value = false;
    renderingPackets.value = true;
    packetPromise.value = undefined;

    // remove all un-whitelisted tokens from the html
    packetHtml.value = response;
    renderingPackets.value = false;

    await nextTick(); // wait until session packets are rendered
    // tooltips for src/dst byte images
    if (!packetContainerRef.value) { return; }
    const tss = packetContainerRef.value.getElementsByClassName('session-detail-ts');
    for (let i = 0; i < tss.length; ++i) {
      let timeEl = tss[i];
      const value = timeEl.getAttribute('value');
      timeEl = timeEl.getElementsByClassName('ts-value');
      if (!isNaN(value)) { // only parse value if it's a number (ms from 1970)
        const time = timezoneDateString(
          parseInt(value),
          user.value.settings.timezone,
          user.value.settings.ms
        );
        timeEl[0].innerHTML = time;
      }
    }

    const dstCol = packetContainerRef.value.querySelector('.dstcol > .str');
    const srcCol = packetContainerRef.value.querySelector('.srccol > .str');
    if (dstCol) { dstCol.textContent = t('common.destination'); }
    if (srcCol) { srcCol.textContent = t('common.source'); }

    const bytesEls = packetContainerRef.value.getElementsByClassName('bytes');
    [...bytesEls].forEach(el => el.textContent = t('common.bytes'));

    // tooltips for linked images
    const imgs = packetContainerRef.value.getElementsByClassName('imagetag');
    for (let i = 0; i < imgs.length; ++i) {
      const img = imgs[i];
      let href = img.href;
      href = href.replace('body', 'bodypng');

      const tooltip = document.createElement('span');
      tooltip.className = 'img-tip';
      tooltip.innerHTML = `File Bytes:
        <br>
        <img src="${href}">
      `;

      img.appendChild(tooltip);
    }

    // add listeners to fetch the src/dst bytes images on mouse enter
    const srcBytes = packetContainerRef.value.getElementsByClassName('srccol');
    if (srcBytes && srcBytes.length) {
      srcBytes[0].addEventListener('mouseenter', showSrcBytesImg);
    }

    const dstBytes = packetContainerRef.value.getElementsByClassName('dstcol');
    if (dstBytes && dstBytes.length) {
      dstBytes[0].addEventListener('mouseenter', showDstBytesImg);
    }

    renderingPackets.value = false;
  } catch (err) {
    loadingPackets.value = false;
    errorPackets.value = err.text || err;
    packetPromise.value = undefined;
  }
};

// Fetch /tshark NDJSON and decode line by line.
const getTshark = async () => {
  if (user.value.hidePcap || !hasTshark.value) { return; }
  if (tsharkLoading.value) { return; }

  tsharkLoading.value = true;
  tsharkError.value = '';
  tsharkPackets.value = [];

  try {
    const { controller, fetcher } = SessionsService.getTshark(
      props.session.id,
      props.session.node,
      props.session.cluster,
      {
        length: tsharkLength.value || 50,
        payload: tsharkPayload.value ? 'true' : 'false',
        hidden: tsharkHidden.value ? 'true' : 'false'
      }
    );
    tsharkPromise.value = { controller };

    const text = await fetcher;
    const out = [];
    for (const line of String(text || '').split('\n')) {
      const trimmed = line.trim();
      if (!trimmed) { continue; }
      try { out.push(JSON.parse(trimmed)); } catch (e) { /* skip non-json line */ }
    }
    tsharkPackets.value = out;
    tsharkLoaded.value = true;
  } catch (err) {
    tsharkError.value = err.text || err.message || err;
  } finally {
    tsharkLoading.value = false;
    tsharkPromise.value = undefined;
  }
};

// Build a short header line for a tshark packet (frame summary if present).
const packetSummary = (pkt) => {
  if (!pkt || !Array.isArray(pkt.layers)) { return ''; }
  const frame = pkt.layers.find(l => l.name === 'frame');
  if (frame && frame.label) { return frame.label; }
  return pkt.layers.map(l => l.name).filter(Boolean).join(' / ');
};

// Wireshark-like color map for common application/transport protocols. Anything
// not listed falls back to a deterministic hashed hue so each protocol gets a
// consistent color across packets.
const tsharkProtoColors = {
  http: '#9be3a4', http2: '#9be3a4', http3: '#9be3a4',
  tls: '#f4d76b', ssl: '#f4d76b', quic: '#f4d76b',
  dns: '#b5d6ff', mdns: '#b5d6ff', llmnr: '#b5d6ff',
  tcp: '#e0e0e0', udp: '#e0e0e0',
  icmp: '#ffd0a0', icmpv6: '#ffd0a0',
  arp: '#ffc8c8',
  smb: '#d8b6ff', smb2: '#d8b6ff', smb3: '#d8b6ff',
  ssh: '#ffd0e0',
  ftp: '#ffd0e0', 'ftp-data': '#ffd0e0',
  smtp: '#ffd0e0', imap: '#ffd0e0', pop: '#ffd0e0',
  dhcp: '#b5d6ff', dhcpv6: '#b5d6ff', bootp: '#b5d6ff',
  ntp: '#cccccc',
  rtp: '#fff0a0', sip: '#fff0a0',
  ipsec: '#d0d0ff', esp: '#d0d0ff'
};
// Protocols to ignore when picking the "top" application protocol — these are
// always present in a typical stack and aren't interesting.
const tsharkSkipProtos = new Set(['eth', 'ethertype', 'ip', 'ipv6', 'sll', 'sll2', 'null', 'vlan', '802.1q', 'gre', 'ppp']);

const tsharkHashColor = (s) => {
  let h = 0;
  for (let i = 0; i < s.length; i++) { h = (h * 31 + s.charCodeAt(i)) | 0; }
  return `hsl(${Math.abs(h) % 360}, 70%, 80%)`;
};

const packetProtoStack = (pkt) => {
  if (!pkt || !Array.isArray(pkt.layers)) { return []; }
  const frame = pkt.layers.find(l => l.name === 'frame');
  if (frame && Array.isArray(frame.fields)) {
    const protos = frame.fields.find(f => f.name === 'frame.protocols');
    if (protos && protos.show) { return String(protos.show).split(':').filter(Boolean); }
  }
  return pkt.layers.map(l => l.name).filter(Boolean);
};

const packetTopProto = (pkt) => {
  const stack = packetProtoStack(pkt);
  for (let i = stack.length - 1; i >= 0; i--) {
    if (!tsharkSkipProtos.has(stack[i])) { return stack[i]; }
  }
  return stack[stack.length - 1] || '';
};

const packetProtoStyle = (proto) => {
  if (!proto) { return {}; }
  const color = tsharkProtoColors[proto] || tsharkHashColor(proto);
  return { background: color, color: '#000' };
};

// mounted
onMounted(async () => {
  setUserParams();
  const response = await SessionsService.getDecodings();
  decodings.value = response;
  setBrowserParams();
  getPackets();
});

onUnmounted(() => {
  cancelPacketLoad();
  if (packetContainerRef.value) {
    const srcBytes = packetContainerRef.value.getElementsByClassName('srccol');
    if (srcBytes && srcBytes.length) {
      srcBytes[0].removeEventListener('mouseenter', showSrcBytesImg);
    }
    const dstBytes = packetContainerRef.value.getElementsByClassName('dstcol');
    if (dstBytes && dstBytes.length) {
      dstBytes[0].removeEventListener('mouseenter', showDstBytesImg);
    }
  }
});
</script>

<style>
.tshark-section {
  margin-top: 0.5rem;
}
/* number input sits inline with the checkbox labels; arkime-input-control
   alone gives it transparent bg + no border, so we paint a compact box. */
.tshark-section .tshark-length {
  width: 5.5em;
  height: 24px;
  display: inline-block;
  margin-left: 0.25rem;
  padding: 0 6px;
  border: 1px solid rgb(var(--v-theme-neutral));
  border-radius: 3px;
  background-color: rgb(var(--v-theme-background));
  font-size: 0.85rem;
}
.tshark-output {
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 0.85rem;
}
.tshark-output .tshark-packet {
  margin-bottom: 0.25rem;
  border-radius: 3px;
  padding: 1px 4px;
}
.tshark-output .tshark-packet > summary {
  cursor: pointer;
}
.tshark-output .tshark-packet[open] {
  padding-bottom: 4px;
}
.tshark-proto-badge {
  display: inline-block;
  background: rgba(0,0,0,0.15);
  color: #000;
  font-weight: bold;
  font-size: 0.75rem;
  padding: 0 6px;
  border-radius: 3px;
  letter-spacing: 0.04em;
}
.tshark-output .tshark-tree {
  padding-left: 1rem;
}
.tshark-output .tshark-tree ul {
  padding-left: 1.25rem;
}
.tshark-output .tshark-tree li {
  list-style: none;
}

.session-detail {
  display: block;
  margin-left: var(--px-md);
  margin-right: var(--px-md);
}

.packet-options {
  border-top: rgb(var(--v-theme-neutral)) 1px solid;
}

.packet-container .file {
  display: inline-block;
  margin-bottom: 20px;
}

/* image tooltips */
.packet-container .srccol,
.packet-container .dstcol {
  padding-bottom: 10px;
}

.packet-container .srccol:hover,
.packet-container .dstcol:hover,
.packet-container .imagetag:hover {
  position: relative;
}
.packet-container .srccol .src-col-tip,
.packet-container .dstcol .dst-col-tip,
.packet-container .imagetag .img-tip {
  display: none;
}
.packet-container .srccol:hover .src-col-tip,
.packet-container .dstcol:hover .dst-col-tip,
.packet-container .imagetag:hover .img-tip {
  font-size: 12px;
  display: block;
  position: absolute;
  margin: 10px;
  left: 0;
  top: 20px;
  padding: 4px 6px;
  color: white;
  text-align: center;
  background-color: black;
  border-radius: 5px;
  line-height: 1.2;
  z-index: 2;
}
.packet-container .srccol .src-col-tip:before,
.packet-container .dstcol .dst-col-tip:before,
.packet-container .imagetag .img-tip:before {
  content: '';
  display: block;
  width: 0;
  height: 0;
  position: absolute;
  border-left: 8px solid transparent;
  border-right: 8px solid transparent;
  border-bottom: 8px solid black;
  top: -7px;
  left: 8px;
}

.packet-container .srccol .src-col-tip .download-bytes,
.packet-container .dstcol .dst-col-tip .download-bytes,
.packet-container .imagetag .img-tip .download-bytes {
  display: block;
  margin-top: 4px;
  font-size: 0.9rem;
}

/* timestamps */
.packet-container .session-detail-ts {
  display: none;
  color: rgb(var(--v-theme-foreground-accent));
  font-weight: bold;
  padding: 0 4px;
  border-bottom: 1px solid rgb(var(--v-theme-neutral));
}
.packet-container.show-ts .session-detail-ts {
  display: block !important;
}

/* src/dst packet visibility */
.packet-container .sessionsrc {
  visibility: visible;
}
.packet-container .sessiondst {
  visibility: visible;
}
.packet-container.hide-src .sessionsrc {
  height: 0px;
  visibility: hidden;
}
.packet-container.hide-dst .sessiondst {
  height: 0px;
  visibility: hidden;
}

/* packets */
.packet-container pre {
  display: block;
  padding: 3px;
  font-size: 12px;
  word-break: break-all;
  word-wrap: break-word;
  border-radius: 0 0 4px 4px;
  margin-top: -1px;
  color: inherit;
  overflow-y: auto;
  white-space: pre-wrap;
}
.packet-container pre .sessionln {
  color: rgb(var(--v-theme-foreground-accent));
}
/* src/dst packet text colors */
.packet-container .sessiondst {
  color: rgb(var(--v-theme-dst)) !important;
  max-width: 50vw !important;
  word-wrap: break-word;
}
.packet-container .sessionsrc {
  color: rgb(var(--v-theme-src)) !important;
  max-width: 50vw !important;
  word-wrap: break-word;
}

/* list values */
.session-detail dt {
  float: left;
  clear: left;
  width: 160px;
  text-align: right;
  margin-right: 6px;
  line-height: 1.7;
  font-weight: 600;
}

.session-detail dd {
  margin-left: 165px;
}

/* more items link */
.session-detail .show-more-items {
  margin-left: 6px;
  margin-right: 6px;
  font-size: 12px;
  text-decoration: none;
}

/* Session-options toolbar -- flex row of v-btn entries */
.session-detail .session-options {
  border-bottom: 1px solid rgb(var(--v-theme-neutral));
  padding-bottom: 4px;
  padding-top: 4px;
}

/* Strip Vuetify's default chunky button styling on the session-options
   v-btn entries so they read as compact toolbar links rather than
   uppercase pill buttons. */
.session-detail .session-options-btn.v-btn {
  text-transform: none;
  letter-spacing: normal;
  font-weight: 400;
  font-size: 12.5px;
  min-width: 0;
  height: 28px;
  padding: 0 10px;
  background-color: transparent;
  box-shadow: none;
  color: rgb(var(--v-theme-foreground));
}

.session-detail .session-options-btn.v-btn:hover {
  background-color: rgb(var(--v-theme-quaternary-lighter));
  color: rgb(var(--v-theme-button-fg));
}

/* Height (21px) matches the dt's line-height-derived height
   (line-height: 1.7) so labels stay aligned with their dd values. */
.session-detail button.clickable-label {
  margin-top: -2px;
  display: inline-block;
  height: 21px;
  background-color: transparent;
  color: rgb(var(--v-theme-foreground));
  font-size: 11px;
  font-weight: 600;
  line-height: 21px;
  padding: 0 5px 1px 5px;
  border: 1px solid rgb(var(--v-theme-neutral));
  border-radius: 0.25rem;
  cursor: pointer;
  max-width: 160px; /* this gets updated by the dl resizing */
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  vertical-align: baseline;
}

.session-detail button.clickable-label:hover {
  color: #333;
  background-color: rgb(var(--v-theme-neutral));
  border-color: rgb(var(--v-theme-neutral));
}

/* "+" add-tag button rendered as inline HTML by the pug arrayList
   helper (see sessionDetail.pug). */
.session-detail .arkime-tag-add-btn {
  display: inline-block;
  padding: 2px 6px;
  font-size: 11px;
  line-height: 1;
  vertical-align: baseline;
  background-color: rgb(var(--v-theme-secondary));
  color: rgb(var(--v-theme-button-fg));
  border: 1px solid rgb(var(--v-theme-secondary));
  border-radius: 0.2rem;
  cursor: pointer;
}
.session-detail .arkime-tag-add-btn:hover {
  filter: brightness(115%);
}

/* Vuetify v-menu teleports its content to body, so these selectors
   target the v-list by the class we put on it in mixins.pug /
   sessionDetail.pug. Global (not scoped) so it reaches the portal. */
.clickable-label-menu {
  max-height: 280px;
  overflow-y: auto;
}
.clickable-label-menu .v-list-item {
  font-size: 12px;
}

/* dl resizing */
.session-detail-grip {
  width: 5px;
  height: 100%;
  cursor: col-resize;
  position: absolute;
  display: inline-block;
}
dl:hover > .session-detail-grip {
  border-right: 1px dotted rgb(var(--v-theme-neutral)) !important;
}

/* Masonry-style multi-column detail card layout. */
.session-detail-wrapper .session-detail-cards { /* default 2-col */
  column-count: 2;
  -moz-column-count: 2;
}
.session-detail-wrapper.card-columns-1 .session-detail-cards {
  column-count: 1;
  -moz-column-count: 1;
}
.session-detail-wrapper.card-columns-3 .session-detail-cards {
  column-count: 3;
  -moz-column-count: 3;
}
@media (max-width: 1350px) {
  .session-detail-wrapper .session-detail-cards {
    column-count: 1;
    -moz-column-count: 1;
  }
}

/* Each card: keeps content together when columns wrap, gets card chrome */
.session-detail .session-detail-card {
  break-inside: avoid;
  -webkit-column-break-inside: avoid;
  page-break-inside: avoid;
  background-color: rgb(var(--v-theme-background));
  color: rgb(var(--v-theme-foreground));
  border: 1px solid rgb(var(--v-theme-neutral-light));
  border-radius: 4px;
  padding: 1.25rem;
  margin-bottom: 0.75rem;
}

.session-detail .session-detail-card > .session-card-title {
  cursor: pointer;
  padding: 0.5rem;
  margin: -1.25rem;
  margin-bottom: 1.25rem;
  border-radius: 4px 4px 0 0;
  background-color: rgb(var(--v-theme-neutral));
  color: rgb(var(--v-theme-background));
}

.session-detail .session-detail-card > dl {
  margin-bottom: 0rem;
  margin-top: -0.75rem;
  position: relative;
}

/* keep dt and dd at the same height so values align with labels */
.session-detail .session-detail-card dl dt,
.session-detail .session-detail-card dl dd {
  min-height: 24px;
}

.session-detail .session-detail-card > dl > dt:hover + dd,
.session-detail .session-detail-card > dl > dd:hover {
  border-radius: 10px;
  background-color: rgb(var(--v-theme-neutral-lighter));
}

.session-detail .session-detail-card dt,
.session-detail .session-detail-card dd {
  line-height: 1.3;
  margin-bottom: 0 !important;
}

/* detail card collapse/expand chevron */
.session-detail .session-detail-card h4:after {
  float: right;
  content: "\f078";
  font-family: FontAwesome;
}
.session-detail .session-detail-card h4.collapsed:after {
  float: right;
  content: "\f077";
  font-family: FontAwesome;
}
.session-detail .session-detail-card.collapsed {
  padding-bottom: 0;
}
.session-detail .session-detail-card.collapsed > h4 {
  margin-bottom: 0;
  border-radius: 4px;
}
.session-detail .session-detail-card > dl > h4.collapsed {
  margin-bottom: 0;
}

/* `.collapse` is toggled onto the <dl> sibling of the card title by
   collapseSection() in sessionDetailData.js. Bootstrap used to provide
   `display: none`; we have to set it ourselves now. */
.session-detail .session-detail-card > dl.collapse {
  display: none;
}
</style>
