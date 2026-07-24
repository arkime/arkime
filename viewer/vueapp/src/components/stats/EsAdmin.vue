<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="arkime-container-fluid mt-3">
    <arkime-loading v-if="loading && !error" />

    <arkime-error
      v-if="error"
      :message="error" />

    <div v-if="!error">
      <v-alert
        type="warning"
        variant="tonal"
        density="compact">
        <span v-html="$t('stats.esAdmin.warningHtml')" />
      </v-alert>

      <v-alert
        v-if="interactionError"
        type="error"
        variant="tonal"
        density="compact"
        closable
        class="mt-2"
        @click:close="interactionError = ''">
        <strong>{{ $t('common.error') }}:</strong>
        {{ interactionError }}
      </v-alert>

      <v-alert
        v-if="interactionSuccess"
        type="success"
        variant="tonal"
        density="compact"
        closable
        class="mt-2"
        @click:close="interactionSuccess = ''">
        <strong>{{ $t('common.success') }}:</strong>
        {{ interactionSuccess }}
      </v-alert>

      <h3 class="d-flex align-center mt-3">
        <span class="flex-grow-1">{{ $t('stats.esAdmin.esClusterSettings') }}</span>
        <v-btn
          variant="flat"
          size="large"
          color="primary"
          class="ms-1"
          @click="retryFailed"
          id="retryFailed">
          {{ $t('stats.esAdmin.retryFailed') }}
          <v-tooltip activator="#retryFailed">
            {{ $t('stats.esAdmin.retryFailedTip') }}
          </v-tooltip>
        </v-btn>
        <v-btn
          variant="flat"
          size="large"
          color="secondary"
          class="ms-1"
          @click="flush"
          id="flush">
          {{ $t('stats.esAdmin.flush') }}
          <v-tooltip activator="#flush">
            {{ $t('stats.esAdmin.flushTip') }}
          </v-tooltip>
        </v-btn>
        <v-btn
          variant="flat"
          size="large"
          color="tertiary"
          class="ms-1"
          @click="unflood"
          id="unflood">
          {{ $t('stats.esAdmin.unflood') }}
          <v-tooltip activator="#unflood">
            {{ $t('stats.esAdmin.unfloodTip') }}
          </v-tooltip>
        </v-btn>
        <v-btn
          variant="flat"
          size="large"
          color="warning"
          class="ms-1"
          @click="clearCache"
          id="clearCache">
          {{ $t('stats.esAdmin.clearCache') }}
          <v-tooltip activator="#clearCache">
            {{ $t('stats.esAdmin.clearCacheTip') }}
          </v-tooltip>
        </v-btn>
      </h3>

      <hr>

      <v-row
        v-for="setting in settings"
        :key="setting.key"
        class="mt-2">
        <v-col cols="12">
          <div class="arkime-input-group arkime-input-group--fluid">
            <span
              :id="`setting-${setting.key}`"
              class="arkime-input-label">
              {{ setting.name }}
              <v-tooltip :activator="`[id='setting-${setting.key}']`">
                {{ setting.key }}
              </v-tooltip>
            </span>
            <input
              type="text"
              @input="setting.changed = true"
              class="arkime-input-control"
              v-model="setting.current"
              :class="{'is-invalid':setting.error || (setting.key === 'cluster.routing.allocation.enable' && setting.current !== 'all')}">
            <span class="arkime-input-label">
              {{ setting.type }}
              <small class="ms-2">
                (<a
                  :href="setting.url"
                  class="no-decoration"
                  target="_blank">
                  Learn more
                </a>)
              </small>
            </span>
            <v-btn
              v-if="setting.key === 'cluster.routing.allocation.enable' && setting.current !== 'all'"
              color="warning"
              variant="flat"
              size="small"
              density="comfortable"
              class="me-1"
              :id="`restore-${setting.key}`"
              :aria-label="$t('stats.esAdmin.restoreAllocationTip')"
              @click="restoreToAll(setting)">
              <v-icon icon="mdi-undo" />
              <v-tooltip :activator="`[id='restore-${setting.key}']`">
                {{ $t('stats.esAdmin.restoreAllocationTip') }}
              </v-tooltip>
            </v-btn>
            <v-btn
              color="warning"
              variant="flat"
              size="small"
              density="comfortable"
              class="me-1"
              :disabled="!setting.changed"
              @click="cancel(setting)">
              {{ $t('common.cancel') }}
            </v-btn>
            <v-btn
              variant="flat"
              size="small"
              density="comfortable"
              :style="primaryBtnStyle"
              class="me-1"
              :disabled="!setting.changed"
              @click="save(setting)">
              {{ $t('common.save') }}
            </v-btn>
          </div>
          <div
            v-if="setting.error"
            class="form-text text-danger">
            <v-icon icon="mdi-alert" />
            {{ setting.error }}
          </div>
        </v-col>
      </v-row>

      <v-alert
        type="info"
        variant="tonal"
        density="compact"
        class="mt-4 mb-4">
        <span v-html="$t('stats.esAdmin.controlHtml')" />
      </v-alert>
    </div>
  </div>
</template>

<script>
import Utils from '../utils/utils';
import ArkimeError from '../utils/Error.vue';
import ArkimeLoading from '../utils/Loading.vue';
import StatsService from './StatsService.js';
import { resolveMessage } from '@common/resolveI18nMessage';

export default {
  name: 'EsAdmin',
  props: {
    cluster: {
      type: String,
      default: ''
    }
  },
  components: {
    ArkimeError,
    ArkimeLoading
  },
  data: function () {
    return {
      loading: true,
      error: '',
      interactionError: '',
      interactionSuccess: '',
      settings: {},
      query: {
        cluster: this.cluster || undefined
      },
      // Arkime theme-color v-btn styles. Vuetify :color can't take CSS vars.
      primaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-primary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      secondaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-secondary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      tertiaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-tertiary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      quaternaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-quaternary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
    };
  },
  created: function () {
    this.loadData();
  },
  watch: {
    cluster: function () {
      this.query.cluster = this.cluster;
      this.loadData();
    }
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    async save (setting) {
      if (!setting.current.match(setting.regex)) {
        setting.error = `Invalid format, this setting must be: ${setting.type}`;
        return;
      }

      const selection = Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this);
      if (!selection.valid) {
        setting.error = selection.error;
        return;
      }

      const data = {
        key: setting.key,
        value: setting.current
      };

      try {
        await StatsService.setAdmin({ data, params: this.query });
        setting.error = '';
        setting.changed = false;
      } catch (error) {
        setting.error = resolveMessage(error, this.$t);
      }
    },
    async restoreToAll (setting) {
      setting.current = 'all';
      setting.changed = true;
      await this.save(setting);
    },
    async cancel (setting) {
      const selection = Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this);
      if (!selection.valid) {
        setting.error = selection.error;
        return;
      }

      try { // update the changed value with the one that's saved
        const response = await StatsService.getAdmin(this.query);
        setting.error = '';
        for (const resSetting of response) {
          if (resSetting.key === setting.key) {
            setting.current = resSetting.current;
            setting.changed = false;
          }
        }
      } catch (error) {
        setting.error = resolveMessage(error, this.$t);
      }
    },
    async clearCache () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this, 'interactionError').valid) {
        return;
      }

      try {
        const response = await StatsService.clearCacheAdmin(this.query);
        this.interactionSuccess = resolveMessage(response, this.$t);
      } catch (error) {
        this.interactionError = resolveMessage(error, this.$t);
      }
    },
    async unflood () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this, 'interactionError').valid) {
        return;
      }

      try {
        const response = await StatsService.unFloodAdmin(this.query);
        this.interactionSuccess = resolveMessage(response, this.$t);
      } catch (error) {
        this.interactionError = resolveMessage(error, this.$t);
      }
    },
    async flush () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this, 'interactionError').valid) {
        return;
      }

      try {
        const response = await StatsService.flushAdmin(this.query);
        this.interactionSuccess = resolveMessage(response, this.$t);
      } catch (error) {
        this.interactionError = resolveMessage(error, this.$t);
      }
    },
    async retryFailed () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this, 'interactionError').valid) {
        return;
      }

      try {
        const response = await StatsService.rerouteAdmin(this.query);
        this.interactionSuccess = resolveMessage(response, this.$t);
      } catch (error) {
        this.interactionError = resolveMessage(error, this.$t);
      }
    },
    /* helper functions ------------------------------------------ */
    async loadData () {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      this.loading = true;

      try {
        const response = await StatsService.getAdmin(this.query);
        this.error = '';
        this.loading = false;
        this.settings = response;
      } catch (error) {
        this.error = resolveMessage(error, this.$t);
        this.loading = false;
      }
    }
  }
};
</script>
