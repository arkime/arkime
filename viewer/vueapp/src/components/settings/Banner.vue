<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <form id="banner">
    <h3>{{ $t('settings.banner.title') }}</h3>
    <p>{{ $t('settings.banner.info') }}</p>

    <hr>

    <!-- enabled -->
    <v-row>
      <v-col
        tag="label"
        cols="12"
        sm="3"
        class="text-end font-weight-bold align-self-center">
        {{ $t('settings.banner.enabled') }}
      </v-col>
      <v-col
        cols="12"
        sm="9"
        class="align-self-center">
        <v-switch
          v-model="banner.enabled"
          color="primary"
          density="compact"
          hide-details
          :label="banner.enabled ? $t('settings.banner.on') : $t('settings.banner.off')" />
      </v-col>
    </v-row> <!-- /enabled -->

    <!-- type -->
    <v-row>
      <v-col
        tag="label"
        cols="12"
        sm="3"
        class="text-end font-weight-bold align-self-center">
        {{ $t('settings.banner.type') }}
      </v-col>
      <v-col
        cols="12"
        sm="9">
        <v-btn-toggle
          v-model="banner.type"
          density="compact"
          divided
          variant="outlined"
          color="secondary"
          class="d-inline-flex"
          mandatory>
          <v-btn value="info">
            {{ $t('settings.banner.typeInfo') }}
          </v-btn>
          <v-btn value="warning">
            {{ $t('settings.banner.typeWarning') }}
          </v-btn>
          <v-btn value="error">
            {{ $t('settings.banner.typeError') }}
          </v-btn>
        </v-btn-toggle>
      </v-col>
    </v-row> <!-- /type -->

    <!-- message -->
    <v-row>
      <v-col
        tag="label"
        cols="12"
        sm="3"
        class="text-end font-weight-bold align-self-center">
        {{ $t('settings.banner.message') }}
      </v-col>
      <v-col
        cols="12"
        sm="9">
        <v-textarea
          v-model="banner.message"
          variant="outlined"
          density="compact"
          rows="3"
          auto-grow
          counter="1000"
          :maxlength="1000"
          :placeholder="$t('settings.banner.messagePlaceholder')" />
      </v-col>
    </v-row> <!-- /message -->

    <!-- live preview -->
    <v-row v-if="banner.message">
      <v-col
        cols="12"
        sm="3"
        class="text-end font-weight-bold align-self-center">
        {{ $t('settings.banner.preview') }}
      </v-col>
      <v-col
        cols="12"
        sm="9">
        <v-alert
          :type="banner.type"
          variant="tonal"
          density="compact"
          class="rounded-0">
          <span style="white-space: pre-line;">{{ banner.message }}</span>
        </v-alert>
      </v-col>
    </v-row> <!-- /live preview -->

    <!-- save -->
    <v-row>
      <v-col
        cols="12"
        sm="3" />
      <v-col
        cols="12"
        sm="9">
        <v-btn
          variant="flat"
          size="large"
          color="primary"
          :loading="saving"
          @click="save">
          <v-icon
            icon="mdi-content-save"
            class="me-2" />
          {{ $t('common.save') }}
        </v-btn>
      </v-col>
    </v-row> <!-- /save -->
  </form>
</template>

<script>
import SettingsService from './SettingsService';

export default {
  name: 'BannerSettings',
  emits: ['display-message'],
  data () {
    return {
      saving: false,
      banner: { enabled: false, message: '', type: 'info' }
    };
  },
  created () {
    // seed the editor from the store, then refresh from the server
    const current = this.$store.state.banner || {};
    this.banner = {
      enabled: !!current.enabled,
      message: current.message || '',
      type: current.type || 'info'
    };

    SettingsService.getBanner().then((banner) => {
      this.banner = {
        enabled: !!banner.enabled,
        message: banner.message || '',
        type: banner.type || 'info'
      };
    }).catch(() => { /* keep store-seeded values */ });
  },
  methods: {
    save () {
      this.saving = true;
      SettingsService.updateBanner(this.banner).then((response) => {
        this.$emit('display-message', { msg: response.text });
      }).catch((error) => {
        this.$emit('display-message', { msg: error.text, type: 'danger' });
      }).finally(() => {
        this.saving = false;
      });
    }
  }
};
</script>
