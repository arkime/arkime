<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="arkime-container-fluid mt-3">
    <div class="d-flex align-center flex-wrap ga-2 mb-2">
      <h3 class="mb-0">
        {{ $t('viewConfig.title') }}
      </h3>
      <code
        v-if="config?.configFile"
        class="text-caption">{{ config.configFile }}</code>
    </div>

    <!-- where a bare key is looked up, in order -->
    <p
      v-if="config?.defaultSections?.length"
      class="text-medium-emphasis text-caption mb-3">
      {{ $t('viewConfig.appliesInfo', { sections: config.defaultSections.map(s => `[${s}]`).join(' \u2192 ') }) }}
    </p>

    <!-- error -->
    <div
      v-if="error"
      class="info-area vertical-center">
      <div class="text-danger">
        <span class="mdi mdi-alert mdi-24px" />
        {{ error }}
      </div>
    </div> <!-- /error -->

    <!-- totp gate -->
    <v-card
      v-if="needTotp"
      max-width="420"
      class="mx-auto mt-8 pa-4">
      <h4 class="mb-2">
        <v-icon
          start
          icon="mdi-shield-key-outline" />
        {{ $t('viewConfig.totpTitle') }}
      </h4>
      <p class="text-medium-emphasis mb-3">
        {{ $t('viewConfig.totpInfo') }}
      </p>
      <div class="arkime-input-group arkime-input-group--fluid">
        <span class="arkime-input-label">{{ $t('settings.totp.verifyCode') }}</span>
        <input
          ref="totpInput"
          type="text"
          maxlength="6"
          inputmode="numeric"
          class="arkime-input-control"
          v-model="totpCode"
          :placeholder="$t('settings.totp.codePlaceholder')"
          @keyup.enter="verify">
        <v-btn
          color="primary"
          variant="flat"
          size="small"
          density="comfortable"
          class="me-1"
          :loading="verifying"
          :disabled="totpCode?.length !== 6"
          @click="verify">
          {{ $t('viewConfig.unlock') }}
        </v-btn>
      </div>
    </v-card>

    <template v-else>
      <!-- controls -->
      <v-row
        dense
        class="align-center mb-1">
        <v-col
          cols="12"
          md="4">
          <div class="arkime-input-group arkime-input-group--fluid">
            <span class="arkime-input-label arkime-input-label-fw">
              <v-icon icon="mdi-magnify" />
            </span>
            <input
              type="text"
              class="arkime-input-control"
              v-model="filter"
              :placeholder="$t('common.filter')">
            <v-btn
              v-if="filter"
              icon
              variant="text"
              size="x-small"
              density="comfortable"
              class="arkime-input-append-btn"
              :aria-label="$t('common.clear')"
              @click="filter = ''">
              <v-icon icon="mdi-close" />
            </v-btn>
          </div>
        </v-col>
        <v-col cols="auto">
          <v-btn-toggle
            v-model="filterTarget"
            mandatory
            divided
            density="compact"
            variant="outlined"
            color="primary">
            <v-btn value="both">
              {{ $t('viewConfig.both') }}
            </v-btn>
            <v-btn value="key">
              {{ $t('viewConfig.key') }}
            </v-btn>
            <v-btn value="value">
              {{ $t('viewConfig.value') }}
            </v-btn>
          </v-btn-toggle>
        </v-col>
        <template v-if="remote">
          <v-col
            cols="12"
            md="3">
            <v-combobox
              v-model="compareNode"
              clearable
              hide-details
              density="compact"
              variant="outlined"
              :items="nodes"
              :loading="comparing"
              :label="$t('viewConfig.compareNode')"
              @update:model-value="loadRemote" />
          </v-col>
          <v-col
            v-if="remoteConfig"
            cols="auto">
            <v-switch
              v-model="diffOnly"
              hide-details
              density="compact"
              color="primary"
              :label="$t('viewConfig.diffOnly')" />
          </v-col>
          <v-col
            v-if="remoteConfig"
            cols="auto"
            class="text-medium-emphasis text-caption">
            {{ $t('viewConfig.differenceCount', diffCount) }}
            <template v-if="blindCount">
              &middot; {{ $t('viewConfig.redactedCount', blindCount) }}
            </template>
          </v-col>
        </template>
      </v-row>

      <template v-if="loading">
        <slot name="loading">
          <div class="text-center mt-5">
            <span class="mdi mdi-loading mdi-spin mdi-24px" />
            <br>
            {{ $t('common.loading') }}
          </div>
        </slot>
      </template>

      <!-- config -->
      <div
        v-if="!loading && blocks.length"
        class="viewconfig">
        <div
          v-for="block in blocks"
          :key="block.name"
          class="viewconfig-block">
          <div class="viewconfig-section">
            {{ block.title }}
          </div>
          <div
            v-for="entry in block.entries"
            :key="entry.key"
            :class="['viewconfig-row', `viewconfig-${entry.status}`]">
            <span class="viewconfig-key">{{ entry.key }}</span>
            <span
              v-if="entry.status !== 'onlyRemote'"
              class="viewconfig-eq">=</span>
            <span
              v-if="entry.status !== 'onlyRemote'"
              class="viewconfig-value">{{ entry.value }}</span>
            <span
              v-else
              class="viewconfig-flag">; {{ $t('viewConfig.onlyThere') }}</span>
            <span
              v-if="entry.env"
              class="viewconfig-flag">; {{ $t('viewConfig.fromEnv') }}</span>
            <span
              v-if="entry.status === 'blind'"
              class="viewconfig-flag">; {{ $t('viewConfig.notComparable') }}</span>
            <div
              v-else-if="entry.status !== 'same'"
              class="viewconfig-remote">
              <span class="viewconfig-flag">{{ compareNode }}:</span>
              <span
                v-if="entry.status === 'onlyLocal'"
                class="viewconfig-flag">{{ $t('viewConfig.onlyHere') }}</span>
              <span
                v-else
                class="viewconfig-value">{{ entry.remoteValue }}</span>
            </div>
          </div>
        </div>
      </div>
      <div
        v-else-if="!loading"
        class="text-medium-emphasis mt-4">
        {{ $t('viewConfig.noResults') }}
      </div>
    </template>
  </div>
</template>

<script setup>
import { ref, computed, nextTick, onMounted, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { clearGrant, getConfig, getNodeConfig, getNodes, verifyTotp } from './ViewConfigService.js';

const props = defineProps({
  // viewer can ask other nodes for their config over s2s, the other apps can't
  remote: {
    type: Boolean,
    default: false
  }
});

const { t } = useI18n();

const config = ref(null);
const remoteConfig = ref(null);
const nodes = ref([]);
const compareNode = ref(null);
const filter = ref('');
const filterTarget = ref('both');
const diffOnly = ref(true);
const loading = ref(false);
const comparing = ref(false);
const error = ref('');
const needTotp = ref(false);
const totpCode = ref('');
const totpInput = ref(null);
const verifying = ref(false);

watch(needTotp, async (needed) => {
  if (!needed) { return; }
  await nextTick();
  totpInput.value?.focus();
});

onMounted(() => {
  load();
  if (props.remote) {
    getNodes().then((list) => { nodes.value = list; }).catch(() => { /* leave the picker empty */ });
  }
});

async function load () {
  loading.value = true;
  error.value = '';
  try {
    config.value = await getConfig();
    needTotp.value = false;
    if (compareNode.value) { await loadRemote(compareNode.value); }
  } catch (err) {
    if (err.data?.needTotp) {
      clearGrant();
      needTotp.value = true;
      config.value = null;
    } else {
      error.value = err.text || err.message;
    }
  } finally {
    loading.value = false;
  }
}

async function loadRemote (node) {
  remoteConfig.value = null;
  if (!node) { return; }

  comparing.value = true;
  error.value = '';
  try {
    remoteConfig.value = await getNodeConfig(node);
  } catch (err) {
    if (err.data?.needTotp) {
      clearGrant();
      needTotp.value = true;
    } else {
      error.value = err.text || err.message;
      compareNode.value = null;
    }
  } finally {
    comparing.value = false;
  }
}

async function verify () {
  verifying.value = true;
  error.value = '';
  try {
    await verifyTotp(totpCode.value);
    totpCode.value = '';
    needTotp.value = false;
    await load();
  } catch (err) {
    error.value = err.text || err.message;
    totpCode.value = '';
  } finally {
    verifying.value = false;
  }
}

// the id lists the server sends back, as fast lookups
const envKeys = computed(() => new Set(config.value?.envKeys ?? []));
const redactedKeys = computed(() => new Set(config.value?.redacted ?? []));
const remoteRedactedKeys = computed(() => new Set(remoteConfig.value?.redacted ?? []));

function statusOf (id, value, remoteValue) {
  if (!remoteConfig.value) { return 'same'; }
  if (remoteValue === undefined) { return 'onlyLocal'; }
  if (value === undefined) { return 'onlyRemote'; }
  if (value !== remoteValue) { return 'changed'; }
  // two redacted values are equal strings, not necessarily equal secrets
  if (redactedKeys.value.has(id) && remoteRedactedKeys.value.has(id)) { return 'blind'; }
  return 'same';
}

// prefix is how the server ids this entry in its envKeys/redacted lists
function mergeEntries (local, remote, prefix) {
  const keys = [...new Set([...Object.keys(local ?? {}), ...Object.keys(remote ?? {})])].sort();
  return keys.map((key) => {
    const id = `${prefix}.${key}`;
    return {
      key,
      // the section is part of what someone searches for, eg `keks` or
      // `default.password`. An override key is already section.key.
      searchKey: prefix === 'override' ? key : id,
      value: local?.[key],
      remoteValue: remote?.[key],
      status: statusOf(id, local?.[key], remote?.[key]),
      // -o overrides are their own mechanism, they never come from the environment
      env: prefix !== 'override' && envKeys.value.has(id)
    };
  });
}

function matches (entry) {
  const term = (filter.value ?? '').trim().toLowerCase();
  if (!term) { return true; }
  const key = entry.searchKey.toLowerCase();
  const value = `${entry.value ?? ''} ${entry.remoteValue ?? ''}`.toLowerCase();
  if (filterTarget.value === 'key') { return key.includes(term); }
  if (filterTarget.value === 'value') { return value.includes(term); }
  return key.includes(term) || value.includes(term);
}

const blocks = computed(() => {
  if (!config.value) { return []; }

  const out = [];

  const overrides = mergeEntries(config.value.overrides, remoteConfig.value?.overrides, 'override');
  if (overrides.length) {
    out.push({ name: '__overrides', title: t('viewConfig.overrides'), entries: overrides });
  }

  const sectionNames = [...new Set([
    ...Object.keys(config.value.sections ?? {}),
    ...Object.keys(remoteConfig.value?.sections ?? {})
  ])].sort();

  for (const sectionName of sectionNames) {
    out.push({
      name: sectionName,
      title: `[${sectionName}]`,
      entries: mergeEntries(config.value.sections?.[sectionName], remoteConfig.value?.sections?.[sectionName], sectionName)
    });
  }

  return out.map((block) => ({
    ...block,
    entries: block.entries.filter(e => matches(e) &&
      !(diffOnly.value && remoteConfig.value && (e.status === 'same' || e.status === 'blind')))
  })).filter(block => block.entries.length);
});

// counted over everything, not just what the filter left showing
const counts = computed(() => {
  const total = { diff: 0, blind: 0 };
  if (!remoteConfig.value || !config.value) { return total; }

  const sections = [...new Set([
    ...Object.keys(config.value.sections ?? {}),
    ...Object.keys(remoteConfig.value.sections ?? {})
  ])];

  const all = [mergeEntries(config.value.overrides, remoteConfig.value.overrides, 'override')];
  for (const section of sections) {
    all.push(mergeEntries(config.value.sections?.[section], remoteConfig.value.sections?.[section], section));
  }

  for (const entry of all.flat()) {
    if (entry.status === 'blind') { total.blind++; } else if (entry.status !== 'same') { total.diff++; }
  }
  return total;
});

const diffCount = computed(() => counts.value.diff);
const blindCount = computed(() => counts.value.blind);
</script>

<style scoped>
.viewconfig {
  font-family: monospace;
  font-size: 0.85rem;
  line-height: 1.5;
  word-break: break-all;
}
.viewconfig-block {
  margin-bottom: 0.75rem;
}
.viewconfig-section {
  font-weight: bold;
  color: rgb(var(--v-theme-primary));
}
.viewconfig-row {
  padding-left: 1rem;
  border-left: 3px solid transparent;
}
.viewconfig-key {
  color: rgb(var(--v-theme-secondary));
}
.viewconfig-eq {
  opacity: 0.6;
}
.viewconfig-flag {
  opacity: 0.6;
  font-style: italic;
  margin-left: 0.5rem;
}
.viewconfig-eq,
.viewconfig-eq + .viewconfig-value {
  margin-left: 0;
}
.viewconfig-remote {
  padding-left: 1rem;
}
.viewconfig-remote .viewconfig-value {
  margin-left: 0.35rem;
}
.viewconfig-changed {
  border-left-color: rgb(var(--v-theme-warning));
}
.viewconfig-onlyLocal {
  border-left-color: rgb(var(--v-theme-success));
}
.viewconfig-onlyRemote {
  border-left-color: rgb(var(--v-theme-info));
}
.viewconfig-blind {
  border-left-color: rgb(var(--v-theme-info));
  opacity: 0.8;
}
</style>
