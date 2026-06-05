<template>
  <div
    :ref="session.id"
    :id="`${session.id}-detail`"
    :class="['session-detail-wrapper', `card-columns-${numCols}`, 'mb-2']">
    <!-- detail error -->
    <h5
      v-if="error"
      class="text-danger mt-3 mb-3 ms-2">
      <v-icon
        icon="mdi-alert"
        class="me-2" />
      {{ error }}
    </h5> <!-- /detail error -->

    <!-- tabs: Details / Packets / tshark. Tab bar always renders; individual
         tabs hide based on user perms (hidePackets, user.hidePcap) and
         tshark availability so an admin user without pcap still sees just
         the Details tab cleanly. -->
    <v-tabs
      v-model="activeTab"
      density="compact"
      color="primary"
      class="session-detail-tabs mt-1">
      <v-tab value="details">
        <v-icon
          icon="mdi-information-outline"
          size="small"
          class="me-1" />
        Details
      </v-tab>
      <v-tab
        v-if="!hidePackets && !user.hidePcap"
        value="packets">
        <v-icon
          icon="mdi-package-variant-closed"
          size="small"
          class="me-1" />
        Packets
      </v-tab>
      <v-tab
        v-if="hasTshark && !hidePackets && !user.hidePcap"
        value="tshark">
        <v-icon
          icon="mdi-magnify-scan"
          size="small"
          class="me-1" />
        Shark
        <span
          v-if="tsharkPackets.length"
          class="ms-2 small text-medium-emphasis">{{ tsharkPackets.length }}</span>
      </v-tab>
    </v-tabs>

    <!-- details tab content -->
    <div v-show="activeTab === 'details'">
      <SessionDetailDataComponent
        :key="componentKey"
        @reload="reload"
        @toggle-col-vis="toggleColVis"
        @toggle-info-vis="toggleInfoVis" />
    </div>

    <!-- packets / tshark area -->
    <div
      v-show="!hidePackets && !user.hidePcap"
      class="session-detail-pcap-area">
      <!-- packets tab content -->
      <div v-show="activeTab === 'packets'">
        <!-- packet options (top) -->
        <fieldset
          class="arkime-pcap-toolbar me-1 ms-1 mt-2 mb-2"
          :disabled="hidePackets || loadingPackets || renderingPackets">
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

        <!-- packets loading -->
        <div
          v-if="loadingPackets"
          class="mt-4 mb-4 ms-2 me-2 large">
          <v-icon
            icon="mdi-loading"
            class="mdi-spin" />&nbsp;
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
        </div>

        <!-- packets rendering -->
        <div
          v-if="renderingPackets"
          class="mt-4 mb-4 ms-2 me-2 large">
          <v-icon
            icon="mdi-loading"
            class="mdi-spin" />&nbsp;
          {{ $t('sessions.detail.renderingSessionPackets') }}&nbsp;
        </div>

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
        </div>

        <!-- packets -->
        <div
          v-if="!loadingPackets && !errorPackets"
          class="inner packet-container me-1 ms-1"
          v-html="packetHtml"
          ref="packetContainerRef"
          :class="{'show-ts':params.ts,'hide-src':!params.showSrc,'hide-dst':!params.showDst}" />

        <!-- packet options (bottom) -->
        <fieldset
          class="arkime-pcap-toolbar me-1 ms-1 mt-2"
          :disabled="hidePackets || loadingPackets || renderingPackets">
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
      </div> <!-- /packets tab -->

      <!-- tshark tab content -->
      <div
        v-if="hasTshark"
        v-show="activeTab === 'tshark'"
        class="tshark-section me-1 ms-1 mt-3">
        <!-- tshark toolbar -->
        <div class="arkime-pcap-toolbar mb-2">
          <!-- running state + cancel (no manual run/refresh -- tshark
               auto-fetches the first time the tab is activated) -->
          <div
            v-if="tsharkLoading"
            class="tb-group">
            <span class="d-inline-flex align-center small text-medium-emphasis">
              <v-icon
                icon="mdi-loading"
                class="mdi-spin me-1" /> running…
            </span>
            <v-btn
              color="warning"
              variant="text"
              @click="cancelTshark">
              <v-icon
                icon="mdi-cancel"
                class="me-1" />
              cancel
            </v-btn>
          </div>

          <!-- filter (leftmost) -->
          <div
            v-if="tsharkPackets.length"
            class="tb-group">
            <v-text-field
              v-model="tsharkFilter"
              placeholder="filter packets…"
              prepend-inner-icon="mdi-magnify"
              clearable
              density="compact"
              variant="outlined"
              hide-details
              class="tshark-filter-input" />
          </div>

          <!-- spacer pushes right-side groups out -->
          <div class="tb-spacer" />

          <!-- packet count -- same v-menu+v-btn pattern PacketOptions uses -->
          <div class="tb-group">
            <v-menu location="bottom end">
              <template #activator="{ props: activatorProps }">
                <v-btn
                  v-bind="activatorProps"
                  variant="text"
                  class="packet-options-select-btn">
                  {{ tsharkLength }} packets
                  <v-icon
                    icon="mdi-menu-down"
                    class="ms-1" />
                </v-btn>
              </template>
              <v-list density="compact">
                <v-list-item
                  v-for="n in [50, 200, 500, 1000, 2000]"
                  :key="n"
                  @click="tsharkLength = n">
                  {{ n }} packets
                </v-list-item>
              </v-list>
            </v-menu>
          </div>

          <!-- expand / collapse -->
          <div
            v-if="tsharkPackets.length"
            class="tb-group">
            <v-btn
              variant="text"
              title="Expand all fields"
              @click="tsharkExpandAll(true)">
              <v-icon
                icon="mdi-unfold-more-horizontal"
                class="me-1" />
              expand
            </v-btn>
            <v-btn
              variant="text"
              title="Collapse all fields"
              @click="tsharkExpandAll(false)">
              <v-icon
                icon="mdi-unfold-less-horizontal"
                class="me-1" />
              collapse
            </v-btn>
          </div>
        </div>

        <!-- protocol histogram -->
        <div
          v-if="tsharkPackets.length"
          class="d-flex flex-wrap gap-1 mb-2 tshark-histogram">
          <v-chip
            v-for="[proto, count] in tsharkProtoCounts"
            :key="proto"
            size="x-small"
            variant="flat"
            label
            :style="packetProtoStyle(proto)"
            @click="tsharkFilter = (tsharkFilter === proto) ? '' : proto">
            {{ proto.toUpperCase() }}&nbsp;×{{ count }}
          </v-chip>
        </div>

        <v-alert
          v-if="tsharkError"
          type="error"
          variant="tonal"
          density="compact"
          class="mb-2">
          {{ tsharkError }}
        </v-alert>

        <!-- empty state (auto-run watcher fires on tab activation, so this
             only shows in the rare race where the watcher hasn't kicked off
             a fetch yet, or if the session simply has no tshark output) -->
        <div
          v-if="!tsharkLoading && tsharkLoaded && !tsharkError && !tsharkPackets.length"
          class="text-medium-emphasis text-center my-6">
          <v-icon
            icon="mdi-magnify-scan"
            size="x-large"
            class="d-block mx-auto mb-2" />
          No tshark output for this session.
        </div>

        <!-- split view: packet list (left) + tree pane (right) -->
        <div
          v-if="tsharkPackets.length"
          ref="tsharkOutputRef"
          class="tshark-split"
          :style="{ gridTemplateColumns: `${tsharkSplitWidth}px 6px 1fr` }">
          <div class="tshark-list">
            <div
              v-for="entry in filteredTsharkPackets"
              :key="entry.origIdx"
              class="tshark-list-item"
              :class="{ 'tshark-list-item--selected': tsharkSelectedIdx === entry.origIdx }"
              :style="packetProtoStyle(packetTopProto(entry.pkt))"
              @click="setTsharkSelected(entry.origIdx)">
              <span class="tshark-list-idx">#{{ entry.origIdx + 1 }}</span>
              <span
                v-if="packetTopProto(entry.pkt)"
                class="tshark-proto-badge ms-1">{{ packetTopProto(entry.pkt).toUpperCase() }}</span>
              <span class="ms-2 small text-truncate">{{ packetSummary(entry.pkt) }}</span>
            </div>
            <div
              v-if="!filteredTsharkPackets.length"
              class="text-medium-emphasis small p-2">
              No packets match "{{ tsharkFilter }}"
            </div>
          </div>

          <div
            class="tshark-split-handle"
            title="Drag to resize"
            @mousedown="startTsharkResize" />

          <div class="tshark-tree-pane">
            <div
              v-if="tsharkSelectedPacket"
              class="tshark-tree-header"
              :style="packetProtoStyle(packetTopProto(tsharkSelectedPacket))">
              <strong>Packet {{ tsharkSelectedIdx + 1 }}</strong>
              <span
                v-if="packetTopProto(tsharkSelectedPacket)"
                class="tshark-proto-badge ms-2">{{ packetTopProto(tsharkSelectedPacket).toUpperCase() }}</span>
              <span class="ms-2 small">{{ packetSummary(tsharkSelectedPacket) }}</span>
            </div>
            <ul
              v-if="tsharkSelectedPacket"
              class="tshark-tree">
              <tshark-node
                v-for="(layer, li) in tsharkSelectedPacket.layers"
                :key="li"
                :node="layer"
                :expand-signal="tsharkExpandSignal" />
            </ul>
          </div>
        </div>
      </div> <!-- /tshark tab -->
    </div>
  </div>
</template>

<script setup>
// external imports
import { ref, defineAsyncComponent, computed, onMounted, nextTick, onUnmounted, inject, watch } from 'vue';
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
const tsharkPromise = ref();
const tsharkOutputRef = ref(null);
const activeTab = ref('details');
const tsharkFilter = ref('');
const tsharkSelectedIdx = ref(0);
const tsharkExpandSignal = ref(0);
const tsharkSplitWidth = ref(280);
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
  tsharkSelectedIdx.value = 0;

  try {
    const { controller, fetcher } = SessionsService.getTshark(
      props.session.id,
      props.session.node,
      props.session.cluster,
      {
        length: tsharkLength.value || 50
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
    // Aborted requests aren't errors from the user's POV — leave error blank.
    const aborted = err?.name === 'AbortError' || /aborted/i.test(err?.message || '');
    if (!aborted) { tsharkError.value = err.text || err.message || err; }
  } finally {
    tsharkLoading.value = false;
    tsharkPromise.value = undefined;
  }
};

const cancelTshark = () => {
  if (tsharkPromise.value?.controller) {
    try { tsharkPromise.value.controller.abort(); } catch (e) { /* ignore */ }
  }
};

// Filter packets by text against summary/protocol/layer names.
const filteredTsharkPackets = computed(() => {
  const list = tsharkPackets.value.map((p, i) => ({ pkt: p, origIdx: i }));
  // v-text-field clearable sets the model to null, so coerce before trim().
  const f = (tsharkFilter.value || '').trim().toLowerCase();
  if (!f) { return list; }
  return list.filter(({ pkt }) => {
    if (packetTopProto(pkt).toLowerCase().includes(f)) { return true; }
    if (packetSummary(pkt).toLowerCase().includes(f)) { return true; }
    return (pkt.layers || []).some(l => (l.name || '').toLowerCase().includes(f));
  });
});

const tsharkSelectedPacket = computed(() => {
  return tsharkPackets.value[tsharkSelectedIdx.value] || null;
});

// Histogram of top-level protocols for the chip strip above the list.
const tsharkProtoCounts = computed(() => {
  const counts = new Map();
  for (const pkt of tsharkPackets.value) {
    const p = (packetTopProto(pkt) || 'other').toLowerCase();
    counts.set(p, (counts.get(p) || 0) + 1);
  }
  return [...counts.entries()].sort((a, b) => b[1] - a[1]);
});

const setTsharkSelected = (idx) => {
  tsharkSelectedIdx.value = idx;
};

// Drag the divider between the packet list and the tree pane.
const startTsharkResize = (e) => {
  e.preventDefault();
  const startX = e.clientX;
  const startW = tsharkSplitWidth.value;
  const onMove = (ev) => {
    tsharkSplitWidth.value = Math.max(160, Math.min(640, startW + (ev.clientX - startX)));
  };
  const onUp = () => {
    window.removeEventListener('mousemove', onMove);
    window.removeEventListener('mouseup', onUp);
    document.body.style.userSelect = '';
    document.body.style.cursor = '';
  };
  window.addEventListener('mousemove', onMove);
  window.addEventListener('mouseup', onUp);
  document.body.style.userSelect = 'none';
  document.body.style.cursor = 'col-resize';
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

// Bump signal so TsharkNode children set their <details> open/closed.
// Positive value → open, negative → close. Increment magnitude so each
// click is a new value even if direction is the same.
const tsharkExpandAll = (expanded) => {
  const base = Math.abs(tsharkExpandSignal.value) + 1;
  tsharkExpandSignal.value = expanded ? base : -base;
};

const packetProtoStyle = (proto) => {
  if (!proto) { return {}; }
  const color = tsharkProtoColors[proto] || tsharkHashColor(proto);
  return { background: color, color: '#000' };
};

// Auto-run tshark the first time the user clicks the tshark tab. No manual
// run/refresh button -- the user opens the tab, results stream in.
watch(activeTab, (newTab) => {
  if (newTab === 'tshark' && hasTshark.value && !tsharkLoaded.value && !tsharkLoading.value) {
    getTshark();
  }
});

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
/* tab strip between detail-fields and packets/tshark content */
.session-detail-tabs {
  border-bottom: 1px solid rgb(var(--v-theme-neutral));
  min-height: 36px;
}
.session-detail-tabs .v-tab {
  text-transform: none;
  letter-spacing: 0;
  font-weight: 600;
  min-width: 0;
  padding-inline: 12px;
}

.tshark-section {
  margin-top: 0.5rem;
}

/* =============================================================
 * Shared packet-detail toolbar — used by the Packets tab
 * (wraps PacketOptions) and the tshark tab. Mirrors the
 * visualization toolbar (session-graph-btn-container): translucent
 * blurred bg, rounded, with each child group separated by a
 * vertical divider. Always-visible (no slide-down).
 * ============================================================= */
.arkime-pcap-toolbar {
  display: flex;
  flex-wrap: wrap;
  align-items: stretch;
  gap: 0;
  padding: 2px 4px;
  background: color-mix(in srgb, rgb(var(--v-theme-background)) 60%, transparent);
  backdrop-filter: blur(4px);
  -webkit-backdrop-filter: blur(4px);
  border-radius: 6px;
  border: 1px solid color-mix(in srgb, rgb(var(--v-theme-foreground)) 14%, transparent);
  box-shadow:
    inset 0 1px 0 rgba(255, 255, 255, 0.10),
    0 1px 2px rgba(0, 0, 0, 0.08);
  /* fieldset reset (used to wrap PacketOptions for disable-cascade) */
  margin-inline-end: 0;
  margin-inline-start: 0;
}
.arkime-pcap-toolbar .tb-group,
.arkime-pcap-toolbar .packet-options-row > .tb-group {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 0 12px;
  min-height: 30px;
  margin: 0 !important;
}
/* divider between adjacent groups, regardless of whether they're
   direct children of the toolbar or nested inside .packet-options-row */
.arkime-pcap-toolbar .tb-group + .tb-group,
.arkime-pcap-toolbar .packet-options-row > .tb-group + .tb-group {
  border-left: 1px solid color-mix(in srgb, rgb(var(--v-theme-foreground)) 18%, transparent);
}
.arkime-pcap-toolbar .tb-spacer {
  flex: 1 1 auto;
  min-width: 0;
}
/* PacketOptions wrapping row already has its own internal layout;
   when it sits inside .arkime-pcap-toolbar make it take full width
   so its groups flow normally. */
.arkime-pcap-toolbar .packet-options-row {
  flex: 1 1 auto;
  min-width: 0;
  width: 100%;
}

/* Tighter v-input chrome inside the toolbar — labels, checkboxes,
   number/filter fields all snap to the toolbar's 30px row height. */
.arkime-pcap-toolbar :deep(.v-selection-control) {
  min-height: 0;
  flex: 0 0 auto;
}
.arkime-pcap-toolbar :deep(.v-checkbox .v-selection-control__wrapper),
.arkime-pcap-toolbar :deep(.v-checkbox .v-selection-control__input) {
  height: 24px;
  width: 24px;
}
.arkime-pcap-toolbar :deep(.v-label) {
  font-size: 12px;
  opacity: 0.9;
}
.arkime-pcap-toolbar .tshark-filter-input {
  flex: 0 0 22rem;
  max-width: 22rem;
}
.arkime-pcap-toolbar .tshark-filter-input :deep(.v-field__input) {
  font-size: 12px;
  padding-top: 2px;
  padding-bottom: 2px;
  min-height: 0;
}
.arkime-pcap-toolbar :deep(.v-btn) {
  min-width: 28px;
  letter-spacing: 0;
  text-transform: none;
}

.tshark-histogram .v-chip {
  cursor: pointer;
  font-weight: 600;
  letter-spacing: 0.03em;
}

/* split view: packet list (resizable) + grab handle + flexible tree pane */
.tshark-split {
  display: grid;
  /* grid-template-columns is set inline so the divider can be dragged */
  border: 1px solid rgb(var(--v-theme-neutral));
  border-radius: 4px;
  overflow: hidden;
  min-height: 280px;
}
.tshark-list {
  background: rgb(var(--v-theme-background));
}
.tshark-split-handle {
  cursor: col-resize;
  background: rgb(var(--v-theme-neutral));
  position: relative;
  transition: background 0.15s;
}
.tshark-split-handle::before {
  /* two faint dots to hint at drag */
  content: '';
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  width: 2px;
  height: 24px;
  background: rgba(0, 0, 0, 0.35);
  box-shadow: 0 0 0 1px rgba(255, 255, 255, 0.2);
  border-radius: 1px;
}
.tshark-split-handle:hover,
.tshark-split-handle:active {
  background: rgb(var(--v-theme-primary));
}
.tshark-list-item {
  padding: 2px 8px;
  cursor: pointer;
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 0.8rem;
  border-bottom: 1px solid rgba(0, 0, 0, 0.08);
  display: flex;
  align-items: center;
  white-space: nowrap;
  overflow: hidden;
}
.tshark-list-item .tshark-list-idx {
  font-weight: 700;
  min-width: 3em;
}
.tshark-list-item .text-truncate {
  overflow: hidden;
  text-overflow: ellipsis;
  flex: 1 1 auto;
  min-width: 0;
}
.tshark-list-item:hover {
  filter: brightness(0.92);
}
.tshark-list-item--selected {
  outline: 2px solid rgb(var(--v-theme-primary));
  outline-offset: -2px;
}

.tshark-tree-pane {
  padding: 0.25rem 0.5rem;
}
.tshark-tree-header {
  padding: 4px 8px;
  border-radius: 3px;
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 0.85rem;
  margin-bottom: 0.5rem;
}
.tshark-proto-badge {
  display: inline-block;
  background: rgba(0,0,0,0.15);
  color: #000;
  font-weight: bold;
  font-size: 0.7rem;
  padding: 0 5px;
  border-radius: 3px;
  letter-spacing: 0.04em;
}
.tshark-tree {
  padding-left: 0.5rem;
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 0.85rem;
}
.tshark-tree ul {
  padding-left: 1.25rem;
}
.tshark-tree li {
  list-style: none;
}
.tshark-tree summary {
  cursor: pointer;
}

.session-detail {
  display: block;
  margin-left: var(--px-md);
  margin-right: var(--px-md);
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

/* Two-column packet grid (replaces the Bootstrap .row / .col-md-6 /
   .offset-md-6 markup the pug template used to emit). src packets
   land in column 1, dst packets in column 2; the unused column is
   left empty by the grid so we don't need filler divs. */
.packet-container .packet-row {
  display: grid;
  grid-template-columns: 1fr 1fr;
}
.packet-container .packet-cell--src {
  grid-column: 1;
  min-width: 0;
}
.packet-container .packet-cell--dst {
  grid-column: 2;
  min-width: 0;
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
/* src/dst packet text colors (positioning is now done by the
   .packet-row grid; widths come from grid columns, not 50vw). */
.packet-container .sessiondst {
  color: rgb(var(--v-theme-dst)) !important;
  word-wrap: break-word;
}
.packet-container .sessionsrc {
  color: rgb(var(--v-theme-src)) !important;
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

/* Strip Vuetify's default chunky button styling on the session-options
   v-btn entries so they read as compact toolbar links rather than
   uppercase pill buttons. The wrapping .arkime-pcap-toolbar (shared with
   the Packets + tshark toolbars) supplies the bar background and border. */
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
  flex: 0 0 auto;
  white-space: nowrap;
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
/* Only show the grip for the <dl> you're actually pointing at. Subsection
   <dl>s are nested inside the card's outer <dl>, so hovering a subsection
   also triggers :hover on the outer <dl> — which would light up its
   full-height, card-spanning grip on top of the subsection's. Restricting
   to the innermost hovered <dl> (:not(:has(dl:hover))) keeps the drag line
   matched to the subsection you're over. */
dl:hover:not(:has(dl:hover)) > .session-detail-grip {
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

/* Subsections nested inside a card's <dl> (DNS "Query", HTTP/email
   "... Header Detail"). They render as a bold header + its own <dl>
   nested inside the top-level <dl>, so the `> .session-card-title` /
   `> dl` spacing above doesn't reach them. Without this the nested
   header keeps its default margins and leaves an extra gap above its
   content. Give the header a little room above and tuck its content
   directly beneath it. */
.session-detail .session-detail-card dl > .session-card-title {
  cursor: pointer;
  margin-top: 0.75rem;
  margin-bottom: 0.5rem;
}
.session-detail .session-detail-card dl > dl {
  margin-top: 0;
  margin-bottom: 0;
  /* positioning context for this subsection's own resize grip, so the
     grip's height:100% measures the subsection, not the outer <dl> */
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

/* detail card collapse/expand chevron — MDI codepoints in the MDI
   webfont. Renders as a box if you forget the font-family (FA was
   removed). */
.session-detail .session-detail-card h4:after {
  float: right;
  content: "\F0140"; /* mdi-chevron-down */
  font-family: "Material Design Icons";
}
.session-detail .session-detail-card h4.collapsed:after {
  float: right;
  content: "\F0143"; /* mdi-chevron-up */
  font-family: "Material Design Icons";
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
   `display: none`; we have to set it ourselves now. Use a descendant
   selector (not `>`) so nested subsection lists (DNS "Query", HTTP/email
   header detail) collapse too — `> dl` only matched top-level cards, so
   clicking a subsection chevron toggled the header gap but never hid the
   content. */
.session-detail .session-detail-card dl.collapse {
  display: none;
}
</style>
