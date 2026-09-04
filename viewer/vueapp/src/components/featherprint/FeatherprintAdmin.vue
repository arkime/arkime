<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout>
    <template #chrome>
      <ArkimeCollapsible>
        <div class="page-toolbar">
          <v-row
            dense
            align="center"
            justify="start"
            class="px-1 pt-2 pb-1 page-subnav">
            <v-col cols="auto">
              <h4 class="mb-0">
                {{ $t('navigation.featherprintadmin') }}
              </h4>
            </v-col>
            <v-spacer />
            <v-col cols="auto">
              <v-tooltip
                location="bottom"
                max-width="420">
                <template #activator="{ props }">
                  <!-- the outcome takes over the button label for a few seconds
                       rather than adding a status line to the toolbar -->
                  <v-btn
                    v-bind="props"
                    color="primary"
                    variant="tonal"
                    size="small"
                    density="comfortable"
                    class="fp-scan-btn"
                    :loading="tickBusy"
                    :disabled="!!tickStatus"
                    @click="runTickNow">
                    {{ tickStatus || $t('featherprint.runTickNow') }}
                  </v-btn>
                </template>
                {{ $t('featherprint.runTickNowTip') }}
              </v-tooltip>
            </v-col>
            <v-col cols="auto">
              <v-tooltip location="bottom">
                <template #activator="{ props }">
                  <v-btn
                    v-bind="props"
                    variant="flat"
                    size="small"
                    density="comfortable"
                    :style="tertiaryBtnStyle"
                    @click="refreshAll">
                    <v-icon
                      start
                      icon="mdi-refresh" />
                    {{ $t('common.refresh') }}
                  </v-btn>
                </template>
                {{ $t('featherprint.refreshTip') }}
              </v-tooltip>
            </v-col>
            <v-col
              cols="auto"
              v-if="monitorState">
              <v-tooltip location="bottom">
                <template #activator="{ props }">
                  <v-icon
                    v-bind="props"
                    icon="mdi-progress-clock"
                    size="small"
                    class="cursor-help" />
                </template>
                {{ $t('featherprint.cursorInfo', {
                  cursor: fmtTs(monitorState.lpValue * 1000),
                  first: fmtTs(monitorState.firstProcessedTs),
                  last: fmtTs(monitorState.lastProcessedTs)
                }) }}
              </v-tooltip>
            </v-col>
          </v-row>
        </div>
      </ArkimeCollapsible>
    </template>

    <div class="arkime-container-fluid mt-3">
      <arkime-loading v-if="!adminConfig && !error" />

      <div v-else-if="adminConfig">
        <!-- read-only: these come from the arkime ini ([featherprint-defaults]
             and [featherprint-subnet]) and are parsed once at viewer start -->
        <v-alert
          type="info"
          variant="tonal"
          density="compact"
          class="mb-4">
          {{ $t('featherprint.readOnlyNote') }}
        </v-alert>

        <!-- settings -->
        <v-card
          density="compact"
          class="arkime-card mb-3">
          <v-card-title>{{ $t('featherprint.settingsLabel') }}</v-card-title>
          <v-card-text>
            <!-- content-width fields in one flex row: there are only ~9 short
                 settings, so a 12-col grid wasted most of the width -->
            <div class="fp-settings">
              <div
                v-for="(value, key) in adminConfig.settings"
                :key="key"
                class="fp-field">
                <small class="d-block">{{ key }}</small>
                <v-icon
                  v-if="typeof value === 'boolean'"
                  size="small"
                  :icon="value ? 'mdi-check-circle' : 'mdi-close-circle'"
                  :class="value ? 'text-theme-accent' : 'text-muted-more'" />
                <span v-else-if="value === null || value === ''">&mdash;</span>
                <span v-else>{{ value }}</span>
              </div>
            </div>
          </v-card-text>
        </v-card>

        <!-- defaults: every known flag with its on/off state -->
        <v-card
          density="compact"
          class="arkime-card mb-3">
          <v-card-title>{{ $t('featherprint.defaultsLabel') }}</v-card-title>
          <v-card-text>
            <div class="d-flex flex-wrap ga-1">
              <v-chip
                v-for="(on, flag) in adminConfig.defaults"
                :key="flag"
                size="small"
                label
                :variant="on ? 'flat' : 'outlined'"
                :color="on ? 'primary' : undefined">
                <v-icon
                  start
                  size="x-small"
                  :icon="on ? 'mdi-check' : 'mdi-minus'" />
                {{ flag }}
              </v-chip>
            </div>
          </v-card-text>
        </v-card>

        <!-- subnets: flags rendered in the same +flag/-flag syntax the ini uses -->
        <v-card
          density="compact"
          class="arkime-card">
          <v-card-title>
            {{ $t('featherprint.subnetsCountLabel', { count: adminConfig.subnets.length }) }}
          </v-card-title>
          <v-card-text>
            <table class="arkime-table">
              <thead>
                <tr>
                  <th class="text-start">
                    {{ $t('featherprint.colCidr') }}
                  </th>
                  <th class="text-start">
                    {{ $t('featherprint.colFlags') }}
                  </th>
                </tr>
              </thead>
              <tbody>
                <tr
                  v-for="s in adminConfig.subnets"
                  :key="s.cidr">
                  <td><code>{{ s.cidr }}</code></td>
                  <td>
                    <span
                      v-if="!Object.keys(s.flags || {}).length"
                      class="text-muted-more">&mdash;</span>
                    <v-chip
                      v-for="(on, flag) in s.flags"
                      :key="flag"
                      size="x-small"
                      label
                      class="me-1"
                      :variant="on ? 'flat' : 'outlined'"
                      :color="on ? 'primary' : undefined">
                      {{ (on ? '+' : '-') + flag }}
                    </v-chip>
                  </td>
                </tr>
                <tr v-if="!adminConfig.subnets.length">
                  <td
                    colspan="2"
                    class="text-muted-more">
                    {{ $t('featherprint.noSubnetsConfigured') }}
                  </td>
                </tr>
              </tbody>
            </table>
          </v-card-text>
        </v-card>
      </div>
    </div>

    <v-snackbar
      :model-value="!!error"
      @update:model-value="(val) => { if (!val) { error = null; } }"
      color="error"
      location="bottom"
      timeout="-1"
      variant="flat">
      {{ error }}
      <template #actions>
        <v-btn
          variant="text"
          icon="$close"
          @click="error = null" />
      </template>
    </v-snackbar>
  </page-layout> <!-- /featherprint admin content -->
</template>

<script>
import { fetchWrapper } from '@common/fetchWrapper';
import { fmtTs, TERTIARY_BTN_STYLE } from './featherprintUtils.js';
import { resolveMessage } from '@common/resolveI18nMessage';
import ArkimeLoading from '../utils/Loading.vue';
import PageLayout from '../utils/PageLayout.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';

// how long a scan outcome stays on the button before it reverts
const TICK_STATUS_MS = 5000;

export default {
  name: 'FeatherprintAdmin',
  components: {
    ArkimeLoading,
    PageLayout,
    ArkimeCollapsible
  },
  data () {
    return {
      adminConfig: null,
      monitorState: null,
      tickBusy: false,
      tickStatus: '',
      tickStatusTimer: null,
      error: null
    };
  },
  mounted () {
    this.refreshAll();
  },
  beforeUnmount () {
    clearTimeout(this.tickStatusTimer);
  },
  computed: {
    tertiaryBtnStyle: () => TERTIARY_BTN_STYLE
  },
  methods: {
    fmtTs (ts) {
      return fmtTs(ts, this.$store?.state?.user?.settings);
    },
    refreshAll () {
      this.loadAdminConfig();
      this.loadState();
    },
    async loadState () {
      try {
        const r = await fetchWrapper({ url: 'api/featherprint/state' });
        this.monitorState = r?.state || null;
      } catch { /* state is informational only */ }
    },
    async loadAdminConfig () {
      try {
        const r = await fetchWrapper({ url: 'api/featherprint/config' });
        this.adminConfig = r?.config ?? null;
        this.error = null;
      } catch (err) {
        this.error = this.$t('featherprint.errorLoadAdminConfig', { reason: resolveMessage(err, this.$t) });
      }
    },
    /* Short outcomes land on the button for a few seconds; the long
       explanatory ones (and failures) go to the snackbar, where they can be
       read and dismissed rather than truncating the button. */
    flashTickStatus (msg) {
      clearTimeout(this.tickStatusTimer);
      this.tickStatus = msg;
      this.tickStatusTimer = setTimeout(() => { this.tickStatus = ''; }, TICK_STATUS_MS);
    },
    async runTickNow () {
      this.tickBusy = true;
      clearTimeout(this.tickStatusTimer);
      this.tickStatus = '';
      try {
        const r = await fetchWrapper({
          url: 'api/featherprint/tick',
          method: 'POST',
          data: {}
        });
        if (r?.alreadyRunning) this.flashTickStatus(this.$t('featherprint.tickAlreadyRunning'));
        else if (r?.notPrimary) this.error = this.$t('featherprint.tickNotPrimary');
        else if (r?.monitorDisabled) this.error = this.$t('featherprint.tickMonitorDisabled');
        else this.flashTickStatus(this.$t('featherprint.tickCompleted'));
        this.refreshAll();
      } catch (err) {
        this.error = this.$t('featherprint.tickFailed', { reason: resolveMessage(err, this.$t) });
      } finally {
        this.tickBusy = false;
      }
    }
  }
};
</script>

<style scoped>
/* ~9 short settings fit one row at desktop width; wrap only when they can't.
   Wide column gap so adjacent label/value pairs don't read as one field. */
.fp-settings {
  display: flex;
  flex-wrap: wrap;
  column-gap: 2rem;
  row-gap: 0.5rem;
}

/* the label swaps to the scan outcome for a few seconds -- hold a floor so
   the toolbar doesn't jump when it changes */
.fp-scan-btn {
  min-width: 150px;
}

/* label above the value, no colon */
.fp-field {
  line-height: 1.3;
  white-space: nowrap;
}
</style>
