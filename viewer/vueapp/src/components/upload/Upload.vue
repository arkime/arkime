<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div>
    <div class="sub-navbar">
      <span class="sub-navbar-title">
        <v-icon icon="mdi-tray-arrow-up" />&nbsp;
        {{ $t('uploads.uploadFile') }}
      </span>
      <div class="float-right small toast-container">
        <arkime-toast
          class="me-1"
          :message="msg"
          :type="msgType"
          :done="messageDone" />
      </div>
    </div>

    <div class="container">
      <!-- demo mode -->
      <v-alert
        v-if="demoMode"
        type="warning"
        variant="tonal"
        density="compact">
        {{ $t('uploads.demoMode') }}
      </v-alert> <!-- /demo mode -->

      <v-row>
        <v-col
          cols="12"
          md="6"
          class="offset-md-3">
          <!-- file -->
          <v-file-input
            :label="$t('uploads.pcapFileUpload')"
            v-model="file"
            placeholder="Choose file..." /> <!-- /file -->

          <!-- tag(s) -->
          <div class="mt-2 mb-2">
            <div class="arkime-input-group arkime-input-group--fluid">
              <span class="arkime-input-label">
                {{ $t('uploads.tags') }}:
              </span>
              <input
                type="text"
                v-model="tags"
                class="arkime-input-control"
                :placeholder="$t('uploads.tagsPlaceholder')">
            </div>
          </div> <!-- /tag(s) -->

          <!-- submit/cancel -->
          <v-row class="mt-3">
            <v-col
              cols="12"
              md="12"
              class="text-end">
              <v-btn
                variant="flat"
                size="small"
                density="comfortable"
                :style="primaryBtnStyle"
                type="submit"
                :disabled="!this.file"
                @click="uploadFile">
                <span v-if="!uploading">
                  <v-icon
                    icon="mdi-upload"
                    class="me-1" />
                  {{ $t('common.upload') }}
                </span>
                <span v-else>
                  <v-icon
                    icon="mdi-loading"
                    class="mdi-spin me-1" />
                  {{ $t('common.uploading') }}
                </span>
              </v-btn>
            </v-col>
          </v-row> <!-- /submit/cancel -->
        </v-col>
      </v-row>

      <!-- file upload error -->
      <v-alert
        v-if="error"
        type="error"
        variant="tonal"
        density="compact"
        class="mt-4">
        <div v-html="error" />
      </v-alert> <!-- /file upload error -->
    </div>
  </div>
</template>

<script>
import setReqHeaders from '@common/setReqHeaders';
import ArkimeToast from '../utils/Toast.vue';

export default {
  name: 'ArkimeUpload',
  components: {
    ArkimeToast
  },
  data: function () {
    return {
      file: null,
      tags: '',
      uploading: false,
      error: '',
      msg: '',
      msgType: undefined,
      demoMode: this.$constants.DEMO_MODE,
      // Arkime theme-color v-btn style. Vuetify :color can't take CSS vars.
      primaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-primary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
    };
  },
  methods: {
    uploadFile: async function () {
      this.uploading = true;

      const formData = new FormData();

      formData.append('file', this.file);
      formData.append('tags', this.tags);

      try {
        // NOTE: using native fetch here not the fetchWrapper because you must NOT set the Content-Type header for FormData
        // The browser does it for you INCLUDING the boundary parameter
        const response = await fetch('api/upload', {
          body: formData,
          method: 'POST',
          headers: setReqHeaders({}) // set auth cookies
          // but don't set the Content-Type header for FormData
          // When using FormData, the browser automatically sets the correct Content-Type header including the boundary parameter,
          // which is required for multipart uploads. Setting it manually will break the upload.
        });

        const responseText = await response.text();

        if (!response.ok) {
          throw new Error(responseText || this.$t('uploads.uploadFailed'));
        }

        this.file = null;
        this.tags = '';
        this.error = '';
        this.uploading = false;
        this.msgType = 'success';
        this.msg = this.$t('uploads.uploadWorked');
      } catch (error) {
        this.error = String(error);
        this.uploading = false;
      }
    },
    messageDone: function () {
      this.msg = '';
      this.msgType = undefined;
    }
  }
};
</script>

<style scoped>
.container {
  margin-top: 120px;
  margin-bottom: 50px;
}
</style>
