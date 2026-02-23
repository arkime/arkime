<template>
  <div
    :ref="session.id"
    :id="`${session.id}-detail`"
    :class="['session-detail-wrapper', `card-columns-${numCols}`]">
    <!-- detail error -->
    <h5
      v-if="error"
      class="text-danger mt-3 mb-3 ms-2">
      <span class="fa fa-exclamation-triangle me-2" />
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
      class="packet-options me-1 ms-1">
      <form class="form-inline mb-2 pt-2 border-top">
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
      <span class="fa fa-spinner fa-spin" />&nbsp;
      {{ $t('sessions.detail.loadingSessionPackets') }}&nbsp;
      <button
        type="button"
        @click="cancelPacketLoad"
        class="btn btn-warning btn-xs">
        <span class="fa fa-ban" />&nbsp;
        {{ $t('common.cancel') }}
      </button>
    </div> <!-- /packets loading -->

    <!-- packets rendering -->
    <div
      v-if="renderingPackets && !hidePackets && !user.hidePcap"
      class="mt-4 mb-4 ms-2 me-2 large">
      <span class="fa fa-spinner fa-spin" />&nbsp;
      {{ $t('sessions.detail.renderingSessionPackets') }}&nbsp;
    </div> <!-- /packets rendering -->

    <!-- packets error -->
    <div
      v-if="!error && errorPackets"
      class="mt-4 mb-4 ms-2 me-2 large">
      <span class="text-danger">
        <span class="fa fa-exclamation-triangle" />&nbsp;
        {{ errorPackets }}&nbsp;
      </span>
      <button
        type="button"
        @click="getPackets"
        class="btn btn-success btn-xs">
        <span class="fa fa-refresh" />&nbsp;
        retry
      </button>
    </div> <!-- /packets error -->

    <!-- packets -->
    <div
      v-if="!loadingPackets && !errorPackets && !hidePackets && !user.hidePcap"
      class="inner packet-container me-1 ms-1"
      v-html="packetHtml"
      ref="packetContainerRef"
      :class="{'show-ts':params.ts,'hide-src':!params.showSrc,'hide-dst':!params.showDst}" /> <!-- packets -->

    <!-- packet options -->
    <div
      v-show="!hidePackets && !user.hidePcap"
      class="packet-options me-1 ms-1">
      <form class="form-inline mb-2 pt-2 border-top">
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
import { ref, defineAsyncComponent, computed, onMounted, nextTick, onUnmounted } from 'vue';
// internal imports
import store from '@/store';
import { timezoneDateString } from '@common/vueFilters.js';
import PacketOptions from './PacketOptions.vue';
import SessionsService from './SessionsService';
import sessionDetailData from './sessionDetailData.js';
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
      return sessionDetailData.getVueInstance(response, props.session); // render the session detail data
    } catch (err) {
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
      <span class="fa fa-download"></span>&nbsp;
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
      <span class="fa fa-download"></span>&nbsp;
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
.session-detail {
  display: block;
  margin-left: var(--px-md);
  margin-right: var(--px-md);
}

.packet-options {
  border-top: var(--color-gray) 1px solid;
}

.packet-container .tooltip .tooltip-inner {
  max-width: 400px;
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
  color: var(--color-foreground-accent);
  font-weight: bold;
  padding: 0 4px;
  border-bottom: 1px solid var(--color-gray);
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
  color: var(--color-foreground-accent, green);
}
/* src/dst packet text colors */
.packet-container .sessiondst {
  color: var(--color-dst, #0000FF) !important;
  max-width: 50vw !important;
  word-wrap: break-word;
}
.packet-container .sessionsrc {
  color: var(--color-src, #CA0404) !important;
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

/* top navigation links */
.session-detail .nav-pills {
  border-bottom: 1px solid var(--color-gray);
  padding-bottom: 4px;
  padding-top: 4px;
}

.session-detail .nav-pills > li.nav-item > a,
.session-detail .nav-pills > div.nav-item > button {
  color: var(--color-foreground);
  background-color: transparent;
  border: none;
}

.session-detail .nav-pills > li.nav-item > a:hover,
.session-detail .nav-pills > div.nav-item > button:hover {
  color: var(--color-button, #FFF);
}

.session-detail .nav-pills > li.nav-item > a:hover,
.session-detail .nav-pills > li.nav-item.open > a,
.session-detail .nav-pills > li.nav-item.open > a:hover,
.session-detail .nav-pills > div.nav-item > button:hover,
.session-detail .nav-pills > div.nav-item.open > button,
.session-detail .nav-pills > div.nav-item.open > button:hover  {
  background-color: var(--color-quaternary-lighter);
}

/* clickable labels */
.session-detail .clickable-label {
  margin-top: -2px;
  display: inline-block;
}

.session-detail .clickable-label button.btn {
  height: 21px;
  background-color: transparent;
  font-size: 11px;
  font-weight: 600;
  line-height: 21px;
  padding: 0 5px 1px 5px;
  max-width: 160px; /* this gets updated by the dl resizing */
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.session-detail .clickable-label button.btn:hover {
  color: #333;
  background-color: var(--color-gray);
  border-color: var(--color-gray);
}

.session-detail .clickable-label .dropdown-menu {
  max-height: 280px;
  overflow-y: auto;
}

.session-detail .clickable-label .dropdown-menu .dropdown-item {
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
  border-right: 1px dotted var(--color-gray) !important;
}

/* detail card styles */
.session-detail-wrapper .card-columns { /* default */
  column-count: 2;
  -moz-column-count: 2;
}
.session-detail-wrapper.card-columns-1 .card-columns {
  column-count: 1;
  -moz-column-count: 1;
}
.session-detail-wrapper.card-columns-3 .card-columns {
  column-count: 3;
  -moz-column-count: 3;
}
@media (max-width: 1350px) {
  .session-detail-wrapper .card-columns {
    column-count: 1;
    -moz-column-count: 1;
  }
}
.session-detail .card > .card-body > .card-title {
  cursor: pointer;
  padding: 0.5rem;
  margin: -1.25rem;
  margin-bottom: 1.25rem;
  border-radius: 4px 4px 0 0;
  background-color: var(--color-gray);
  color: var(--color-background, #333);
}
.session-detail .card > .card-body > dl .card-title {
  cursor: pointer;
  font-size: 1rem;
  padding: 0.25rem;
  margin-top: 0.3rem;
  position: relative;
  border-radius: 4px;
  margin-bottom: 1rem;
  color: var(--color-foreground, #333);
  background-color: var(--color-gray-light);
}

.session-detail .card > .card-body dl {
  margin-bottom: 0rem;
  margin-top: -0.75rem;
  position:relative;
}

/* this is required to keep the dt and dd the same height
   so that the values in dd are aligned with their labels in dt */
.session-detail .card > .card-body dl dt,
.session-detail .card > .card-body dl dd {
  min-height: 24px;
}

.session-detail .card > .card-body > dl > dt:hover + dd,
.session-detail .card > .card-body > dl > dd:hover {
  border-radius: 10px;
  background-color: var(--color-gray-lighter);
}

.session-detail .card > .card-body dt,
.session-detail .card > .card-body dd {
  line-height: 1.7;
  margin-bottom: 0.2rem !important;
}

/* detail card collapse/expand */
.session-detail .card h4:after {
  float: right;
  content: "\f078";
  font-family: FontAwesome;
}
.session-detail .card h4.collapsed:after {
  float: right;
  content: "\f077";
  font-family: FontAwesome;
}

.session-detail .card > .card-body.collapsed {
  padding-bottom: 0;
}
.session-detail .card > .card-body.collapsed > h4 {
  margin-bottom: 0;
  border-radius: 4px;
}
.session-detail .card > .card-body > dl > h4.collapsed {
  margin-bottom: 0;
}
</style>
