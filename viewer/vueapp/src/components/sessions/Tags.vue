<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <!-- tag sessions form -->
  <div class="row"
    @keyup.stop.prevent.enter="apply(add)">

    <SegmentSelect :segments.sync="segments" />

    <div class="col-md-5">

      <!-- tags input -->
      <div class="input-group input-group-sm">
        <div class="input-group-prepend">
          <span class="input-group-text">
            Tags
          </span>
        </div>
        <b-form-input
          autofocus
          type="text"
          v-model="tags"
          class="form-control"
          placeholder="Enter a comma separated list of tags"
        />
      </div> <!-- /tags input -->

      <!-- error -->
      <p v-if="error"
        class="small text-danger mb-0">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ error }}
      </p> <!-- /error -->

    </div>

    <!-- buttons -->
    <div class="col-md-3">
      <div class="pull-right">
        <button
          v-if="add"
          type="button"
          title="Add Tags"
          @click="apply(true)"
          :class="{'disabled':loading}"
          class="btn btn-sm btn-theme-tertiary">
          <span v-if="!loading">
            <span class="fa fa-plus-circle">
            </span>&nbsp;
            Add Tags
          </span>
          <span v-else>
            <span class="fa fa-spinner fa-spin">
            </span>&nbsp;
            Adding Tags
          </span>
        </button>
        <button
          v-else
          type="button"
          title="Remove Tags"
          @click="apply(false)"
          :class="{'disabled':loading}"
          class="btn btn-sm btn-danger">
          <span v-if="!loading">
            <span class="fa fa-trash-o">
            </span>&nbsp;
            Remove Tags
          </span>
          <span v-else>
            <span class="fa fa-spinner fa-spin">
            </span>&nbsp;
            Removing Tags
          </span>
        </button>
        <button
          type="button"
          title="cancel"
          v-b-tooltip.hover
          @click="done(null)"
          class="btn btn-sm btn-warning">
          <span class="fa fa-ban">
          </span>
        </button>
      </div>
    </div> <!-- /buttons -->

  </div> <!-- /tag sessions form -->

</template>

<script>
import SessionsService from './SessionsService';
import SegmentSelect from './SegmentSelect';

export default {
  name: 'ArkimeTagSessions',
  components: { SegmentSelect },
  props: {
    add: Boolean,
    start: Number,
    done: Function,
    single: Boolean,
    applyTo: String,
    sessions: Array,
    numVisible: Number,
    numMatching: Number
  },
  data: function () {
    return {
      error: '',
      loading: false,
      segments: 'no',
      tags: ''
    };
  },
  methods: {
    /* exposed functions ----------------------------------------- */
    apply: function (addTags) {
      if (!this.tags) {
        this.error = 'No tag(s) specified.';
        return;
      }

      this.loading = true;

      const data = {
        tags: this.tags,
        start: this.start,
        applyTo: this.applyTo,
        segments: this.segments,
        sessions: this.sessions,
        numVisible: this.numVisible,
        numMatching: this.numMatching
      };

      SessionsService.tag(addTags, data, this.$route.query).then((response) => {
        this.tags = '';
        this.loading = false;
        this.done(response.data.text, response.data.success, this.single);
      }).catch((error) => {
        // display the error under the form so that user
        // has an opportunity to try again (don't close the form)
        this.error = error.text;
        this.loading = false;
      });
    }
  }
};
</script>
