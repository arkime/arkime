<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div>

    <div class="sub-navbar">
      <span class="sub-navbar-title">
        <span class="fa-stack">
          <span class="fa fa-upload fa-stack-1x">
          </span>
          <span class="fa fa-square-o fa-stack-2x">
          </span>
        </span>&nbsp;
        Upload File
      </span>
      <div class="pull-right small toast-container">
        <arkime-toast
          class="me-1"
          :message="msg"
          :type="msgType"
          :done="messageDone">
        </arkime-toast>
      </div>
    </div>

    <div class="container">

      <!-- demo mode -->
      <div v-if="demoMode" class="alert alert-warning">
        <span class="fa fa-exclamation-triangle me-1"></span>
        Everything uploaded will be visible to everyone else using this demo!
      </div> <!-- /demo mode -->

      <div class="row">
        <div class="col-md-6 offset-md-3">

          <!-- file -->
          <BFormFile
            label="PCAP File Upload"
            :model-value="file"
            @update:model-value="(val) => file = val"
          /> <!-- /file -->

          <!-- tag(s) -->
          <div class="form-group mt-2 mb-2">
            <div class="input-group">
              <span class="input-group-text">
                Tag(s)
              </span>
              <input
                type="text"
                v-model="tags"
                class="form-control"
                placeholder="Comma separated list of tags"
              />
            </div>
          </div> <!-- /tag(s) -->

          <!-- submit/cancel -->
          <div class="form-group row">
            <div class="col-md-12">
              <button class="btn btn-theme-primary pull-right ms-1"
                type="submit"
                :disabled="!this.file"
                @click="uploadFile">
                <span v-if="!uploading">
                  <span class="fa fa-upload">
                  </span>&nbsp;
                  Upload
                </span>
                <span v-else>
                  <span class="fa fa-spinner fa-spin">
                  </span>&nbsp;
                  Uploading...
                </span>
              </button>
              <button class="btn btn-warning pull-right"
                :disabled="!this.file"
                @click="cancel">
                <span class="fa fa-ban">
                </span>&nbsp;
                Cancel
              </button>
            </div>
          </div> <!-- /submit/cancel -->

        </div>
      </div>

      <!-- file upload error -->
      <div class="alert alert-danger mt-4"
        v-if="error">
        <div v-html="error"></div>
      </div> <!-- /file upload error -->

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
      file: '',
      tags: '',
      uploading: false,
      error: '',
      msg: '',
      msgType: undefined,
      demoMode: this.$constants.DEMO_MODE
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
        });

        if (!response.ok) {
          const errorText = await response.text();
          throw new Error(errorText || 'File upload failed');
        }

        const data = await response.json();
        if (data.error) {
          throw new Error(data.error);
        }

        this.file = '';
        this.tags = '';
        this.error = '';
        this.uploading = false;
        this.msgType = 'success';
        this.msg = 'File successfully uploaded';
      } catch (error) {
        this.error = error;
        this.uploading = false;
      }
    },
    cancel: function () {
      this.file = '';
      this.tags = '';
      this.error = '';
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
