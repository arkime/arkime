<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<!-- Session action buttons (link/download/source/dest + Columns/Actions
     menus). Rendered as a fragment inside the shared SessionDetail toolbar;
     the action forms + toast are owned by SessionDetail. -->
<template>
  <v-btn
    v-if="actions.rootId"
    class="session-options-btn"
    variant="text"
    size="small"
    @click="allSessions">
    <v-icon
      icon="mdi-source-branch"
      class="me-1" />
    All Sessions
  </v-btn>

  <v-menu>
    <template #activator="{ props: activatorProps }">
      <v-btn
        v-bind="activatorProps"
        class="session-options-btn"
        variant="text"
        size="small">
        <v-icon
          icon="mdi-link"
          class="me-1" />
        {{ $t('sessions.link') }}
        <v-icon
          icon="mdi-menu-down"
          class="ms-1" />
      </v-btn>
    </template>
    <v-list density="compact">
      <v-list-item
        prepend-icon="mdi-clipboard"
        @click="copyLink">
        {{ $t('sessions.copyLink') }}
      </v-list-item>
      <v-list-item
        prepend-icon="mdi-open-in-new"
        :href="permalink"
        target="_blank">
        {{ $t('sessions.openLinkNewTab') }}
      </v-list-item>
      <v-list-item
        prepend-icon="mdi-arrow-right-circle"
        :href="permalink">
        {{ $t('sessions.pivotToLink') }}
      </v-list-item>
    </v-list>
  </v-menu>

  <template v-if="actions.hasPackets">
    <template v-if="actions.rootId">
      <v-btn
        class="session-options-btn"
        variant="text"
        size="small"
        :href="`api/session/${actions.node}/${actions.id}/pcap`"
        :download="`${actions.id}-segment.pcap`">
        <v-icon
          icon="mdi-download"
          class="me-1" />
        {{ $t('sessions.downloadSegmentPCAP') }}
      </v-btn>
      <v-btn
        class="session-options-btn"
        variant="text"
        size="small"
        :href="`api/session/entire/${actions.node}/${actions.rootId}/pcap`"
        :download="`${actions.communityId || actions.rootId}.pcap`">
        <v-icon
          icon="mdi-download"
          class="me-1" />
        {{ $t('sessions.downloadEntirePCAP') }}
      </v-btn>
    </template>
    <v-btn
      v-else-if="canDownloadPcap"
      class="session-options-btn"
      variant="text"
      size="small"
      :href="`api/session/${actions.node}/${actions.id}/pcap`"
      :download="`${actions.id}.pcap`">
      <v-icon
        icon="mdi-download"
        class="me-1" />
      {{ $t('sessions.downloadPCAP') }}
    </v-btn>

    <v-btn
      class="session-options-btn"
      variant="text"
      size="small"
      :href="`api/session/raw/${actions.node}/${actions.id}?type=src`"
      :download="`${actions.id}-src-raw`">
      <v-icon
        icon="mdi-arrow-up-circle"
        class="me-1" />
      {{ $t('sessions.sourceRaw') }}
    </v-btn>
    <v-btn
      class="session-options-btn"
      variant="text"
      size="small"
      :href="`api/session/raw/${actions.node}/${actions.id}?type=dst`"
      :download="`${actions.id}-dst-raw`">
      <v-icon
        icon="mdi-arrow-down-circle"
        class="me-1" />
      {{ $t('sessions.destinationRaw') }}
    </v-btn>
  </template>

  <!-- slot sits between the left action buttons and the right-justified
       Columns/Actions menus (e.g. the detail find box) -->
  <slot />

  <template v-if="showMenus">
    <v-menu>
      <template #activator="{ props: activatorProps }">
        <v-btn
          v-bind="activatorProps"
          class="session-options-btn ms-auto"
          variant="text"
          size="small">
          <v-icon
            icon="mdi-view-column"
            class="me-1" />
          {{ $t('sessions.columns') }}
          <v-icon
            icon="mdi-menu-down"
            class="ms-1" />
        </v-btn>
      </template>
      <v-list density="compact">
        <v-list-item
          prepend-icon="mdi-view-sequential"
          @click="toggleLayout(1)">
          {{ $t('sessions.oneColumn') }}
        </v-list-item>
        <v-list-item
          prepend-icon="mdi-view-column"
          @click="toggleLayout(2)">
          {{ $t('sessions.twoColumn') }}
        </v-list-item>
        <v-list-item
          prepend-icon="mdi-view-grid"
          @click="toggleLayout(3)">
          {{ $t('sessions.threeColumn') }}
        </v-list-item>
      </v-list>
    </v-menu>

    <v-menu>
      <template #activator="{ props: activatorProps }">
        <v-btn
          v-bind="activatorProps"
          class="session-options-btn"
          variant="text"
          size="small">
          <v-icon
            icon="mdi-cog"
            class="me-1" />
          {{ $t('sessions.actions') }}
          <v-icon
            icon="mdi-menu-down"
            class="ms-1" />
        </v-btn>
      </template>
      <v-list density="compact">
        <v-list-item
          v-if="actions.hasPackets && canDownloadPcap"
          prepend-icon="mdi-download"
          @click="emit('openForm', { type: 'export:pcap' })">
          {{ $t('sessions.exports.exportPCAP') }}
        </v-list-item>
        <v-list-item
          prepend-icon="mdi-tag-plus"
          @click="emit('openForm', { type: 'add:tags' })">
          {{ $t('sessions.tag.addTags') }}
        </v-list-item>
        <v-list-item
          v-if="canRemove"
          prepend-icon="mdi-tag-minus"
          @click="emit('openForm', { type: 'remove:tags' })">
          {{ $t('sessions.tag.removeTags') }}
        </v-list-item>
        <v-list-item
          v-if="canRemove"
          prepend-icon="mdi-delete"
          @click="emit('openForm', { type: 'remove:data' })">
          {{ $t('search.removeData') }}
        </v-list-item>
        <v-list-item
          v-for="(value, key) in remoteclusters"
          :key="key"
          prepend-icon="mdi-send"
          @click="emit('openForm', { type: 'send:session', cluster: key })">
          {{ $t('search.sendSession', { name: value.name }) }}
        </v-list-item>
      </v-list>
    </v-menu>
  </template>
</template>

<script setup>
import { computed } from 'vue';
import { useRoute } from 'vue-router';
import { useI18n } from 'vue-i18n';
import qs from 'qs';
import store from '@/store';
import UserService from '../users/UserService';

const props = defineProps({
  actions: { type: Object, required: true },
  showMenus: { type: Boolean, default: false }
});
const emit = defineEmits(['openForm']);

const route = useRoute();
const { t } = useI18n();

const remoteclusters = computed(() => store.state.remoteclusters);
const canDownloadPcap = computed(() => UserService.hasPermission('!disablePcapDownload'));
const canRemove = computed(() => UserService.hasPermission('removeEnabled'));

const permalink = computed(() => {
  const id = props.actions.id.split(':');
  let prefixlessId = id.length > 1 ? id[1] : id[0];
  if (prefixlessId[1] === '@') { prefixlessId = prefixlessId.substr(2); }
  return `sessions?${qs.stringify({
    expression: `id == ${prefixlessId}`,
    startTime: Math.floor(props.actions.firstPacket / 1000),
    stopTime: Math.ceil(props.actions.lastPacket / 1000),
    cluster: props.actions.cluster,
    openAll: 1
  })}`;
});

function copyLink () {
  const url = new URL(permalink.value, document.baseURI).href;
  if (!navigator.clipboard) {
    alert(t('common.clipboardNotSupported', { value: url }));
    return;
  }
  navigator.clipboard.writeText(url);
}

function allSessions () {
  store.commit('setExpression', `rootId == ${props.actions.rootId}`);
  let startTime = Math.floor(props.actions.firstPacket / 1000);
  if (route.query.startTime && route.query.startTime < startTime) {
    startTime = route.query.startTime;
  }
  store.commit('setTime', { startTime });
}

function toggleLayout (numCols) {
  store.commit('setSessionDetailCols', numCols);
}
</script>
