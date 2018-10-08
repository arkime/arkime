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
        <moloch-toast
          class="mr-1"
          :message="msg"
          :type="msgType"
          :done="messageDone">
        </moloch-toast>
      </div>
    </div>

    <div class="container">
      <div class="row">
        <div class="col-md-12">

          <!-- file -->
          <div class="form-group">
            <div class="custom-file">
              <input type="file"
                @change="handleFile"
                class="custom-file-input"
                id="customFile"
                ref="file"
              />
              <label class="custom-file-label"
                for="customFile">
                <span v-if="!file">
                  Choose file
                </span>
                <span v-else>
                  {{ file.name }}
                </span>
              </label>
            </div>
          </div> <!-- /file -->

          <!-- tag(s) -->
          <div class="form-group">
            <div class="input-group">
              <div class="input-group-prepend">
                <span class="input-group-text">
                  Tag(s)
                </span>
              </div>
              <input type="text"
                v-model="tags"
                class="form-control"
                placeholder="Comma separated list of tags"
              />
            </div>
          </div> <!-- /tag(s) -->

          <!-- submit/cancel -->
          <div class="form-group row">
            <div class="col-md-12">
              <button class="btn btn-theme-primary pull-right ml-1"
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
      <moloch-error
        v-if="error"
        :message-html="error"
        class="mt-5 mb-5">
      </moloch-error> <!-- /file upload error -->

    </div>

  </div>

</template>

<script>
import Vue from 'vue';
import MolochToast from '../utils/Toast';
import MolochError from '../utils/Error';

export default {
  name: 'MolochUpload',
  components: { MolochToast, MolochError },
  data: function () {
    return {
      file: '',
      tags: '',
      uploading: false,
      error: '',
      msg: '',
      msgType: undefined
    };
  },
  methods: {
    handleFile: function () {
      this.file = this.$refs.file.files[0];
    },
    uploadFile: function () {
      this.uploading = true;

      let formData = new FormData();

      formData.append('file', this.file);
      formData.append('tags', this.tags);

      Vue.axios.post(
        'upload',
        formData, {
          headers: { 'Content-Type': 'multipart/form-data' }
        }
      ).then((response) => {
        this.file = '';
        this.tags = '';
        this.error = '';
        this.uploading = false;
        this.msgType = 'success';
        this.msg = 'File succesfully uploaded';
      }).catch((error) => {
        this.error = error;
        this.uploading = false;
      });
    },
    cancel: function () {
      this.file = '';
      this.tags = '';
      this.error = '';
    },
    /* remove the message when user is done with it or duration ends */
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
