<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <!-- title / details subnav: the same .sub-navbar band Settings/Config/
         Upload use, so it's the same height/class as the rest of the app -->
    <div
      ref="titlebarEl"
      class="sub-navbar d-flex align-center flex-nowrap">
      <span class="sub-navbar-title text-no-wrap">
        <v-icon
          icon="mdi-cog-outline"
          size="small"
          class="me-1" />
        {{ $t('viewConfig.title') }}
      </span>
      <code
        v-if="config?.configFile"
        class="text-caption text-medium-emphasis ms-3">{{ config.configFile }}</code>
      <v-spacer />
      <!-- where a bare key is looked up, in order -->
      <v-tooltip
        v-if="config?.defaultSections?.length"
        location="bottom">
        <template #activator="{ props: tooltipProps }">
          <v-icon
            v-bind="tooltipProps"
            icon="mdi-information-outline"
            size="small"
            class="text-medium-emphasis" />
        </template>
        {{ $t('viewConfig.appliesInfo', { sections: config.defaultSections.map(s => `[${s}]`).join(' → ') }) }}
      </v-tooltip>
    </div> <!-- /title / details subnav -->

    <!-- inputs subnav: a second band flush under the one above, same
         shape/height, different tint. arkime-toolbar (common.css) pins
         every mixed v-btn/v-input child to 32px -- without it the
         v-combobox renders at Vuetify's default ~40px and sits lower
         than the plain arkime-input-group filter box next to it. -->
    <v-row
      v-if="!needTotp"
      dense
      align="center"
      justify="start"
      class="viewconfig-inputbar arkime-toolbar flex-nowrap"
      :style="{ top: `calc(36px + var(--app-banner-height, 0px) + ${titlebarHeight}px)` }">
      <v-col
        cols="auto"
        class="flex-grow-1"
        style="min-width: 160px;">
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
          cols="auto"
          style="width: 200px;">
          <v-combobox
            v-model="compareNode"
            clearable
            hide-details
            density="compact"
            variant="outlined"
            :items="nodes"
            :loading="comparing"
            :placeholder="$t('viewConfig.compareNode')"
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
          class="text-medium-emphasis text-caption text-no-wrap">
          {{ $t('viewConfig.differenceCount', diffCount) }}
          <template v-if="blindCount">
            &middot; {{ $t('viewConfig.redactedCount', blindCount) }}
          </template>
        </v-col>
      </template>
    </v-row> <!-- /inputs subnav -->

    <div
      class="arkime-container-fluid"
      :style="{ marginTop: `${titlebarHeight + (needTotp ? 0 : 50)}px` }">
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
        <v-card
          v-if="!loading && blocks.length"
          elevation="3"
          class="px-4 py-3 mt-2 mb-2">
          <div class="viewconfig">
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
        </v-card> <!-- /config -->
        <div
          v-else-if="!loading"
          class="text-medium-emphasis mt-4">
          {{ $t('viewConfig.noResults') }}
        </div>
      </template>
    </div> <!-- /arkime-container-fluid -->
  </div>
</template>

<script setup>
import { ref, computed, nextTick, onMounted, onBeforeUnmount, watch } from 'vue';
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

// band 2 sits fixed directly under band 1 (the real .sub-navbar) -- its
// height isn't ours to hardcode, so measure it instead of guessing
const titlebarEl = ref(null);
const titlebarHeight = ref(50);
let titlebarObserver;

onMounted(() => {
  load();
  if (props.remote) {
    getNodes().then((list) => { nodes.value = list; }).catch(() => { /* leave the picker empty */ });
  }

  const measure = () => { titlebarHeight.value = titlebarEl.value.offsetHeight; };
  measure();
  titlebarObserver = new ResizeObserver(measure);
  titlebarObserver.observe(titlebarEl.value);
});

onBeforeUnmount(() => titlebarObserver?.disconnect());

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
/* Band 1 is the real .sub-navbar (common/common.css) -- same class, same
   height, same fixed band Settings/Config/Upload use everywhere else.
   Band 2 sits flush underneath it, same fixed mechanism, offset by
   band 1's actual measured height (titlebarHeight, see script) rather
   than a guessed number -- .sub-navbar's height isn't ours to hardcode
   (locale/font/banner can all change it), and guessing it produced a
   gap. Different tint than band 1, matching the app's existing title
   band vs. controls band colors (Search.vue is secondary-lightest, the
   page-toolbar band under it is quaternary-lightest). */
.viewconfig-inputbar {
  position: fixed;
  left: 0;
  right: 0;
  z-index: 100;
  height: 50px;
  padding: 0 var(--px-md) 0 13px;
  background-color: rgb(var(--v-theme-quaternary-lightest));
  box-shadow: 0 8px 16px -8px black;
}

.viewconfig {
  font-family: monospace;
  font-size: 0.85rem;
  line-height: 1.5;
  word-break: break-all;
}
.viewconfig-block {
  margin-bottom: 0.75rem;
}
.viewconfig-block:last-child {
  margin-bottom: 0;
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
