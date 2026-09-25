<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <v-alert
    :type="alertType"
    :icon="false"
    variant="tonal"
    density="compact"
    class="mb-0">
    <issue-actions
      v-if="isUser"
      class="issue-btns"
      :issue="issue"
      :group-id="groupId"
      :cluster-id="clusterId"
      @issue-change="issueChange" />
    {{ issue.message }}
    <br>
    <small class="cursor-help issue-date">
      {{ moment(issue.lastNoticed || issue.firstNoticed, 'MM/DD HH:mm:ss') }}
      <v-tooltip activator="parent">
        <span v-html="issueDateTooltip(issue)" />
      </v-tooltip>
    </small>
  </v-alert>
</template>

<script>
import IssueActions from './IssueActions.vue';
import moment from 'moment-timezone';

export default {
  name: 'Issue',
  emits: ['issueChange'],
  components: {
    IssueActions
  },
  props: {
    issue: {
      type: Object,
      required: true
    },
    groupId: {
      type: String,
      required: true
    },
    clusterId: {
      type: String,
      required: true
    },
    index: {
      type: Number,
      required: true
    }
  },
  computed: {
    isUser () {
      return this.$store.state.isUser;
    },
    alertType () {
      if (this.issue.severity === 'red') return 'error';
      if (this.issue.severity === 'yellow') return 'warning';
      return 'info';
    }
  },
  methods: {
    moment: function (date, format) {
      return moment(date).format(format);
    },
    issueDateTooltip: function (issue) {
      const firstNoticed = moment(issue.firstNoticed).format('YYYY/MM/DD HH:mm:ss');

      let htmlStr =
      `<small>
        <div>
          ${this.$t('parliament.issue.firstHtml', { first: firstNoticed })}
        </div>`;

      if (issue.lastNoticed) {
        const lastNoticed = moment(issue.lastNoticed).format('YYYY/MM/DD HH:mm:ss');
        htmlStr +=
          `<div>
            ${this.$t('parliament.issue.lastHtml', { last: lastNoticed })}
          </div>`;
      }

      htmlStr += '</small>';
      return htmlStr;
    },
    issueChange: function (changeEvent) {
      // populate the change up to the parliament
      changeEvent.index = this.index;
      this.$emit('issueChange', changeEvent);
    }
  }
};
</script>

<style scoped>
.issue-btns {
  float: right;
  margin-left: 8px;
}

.issue-date {
  color: #676767;
  font-style: italic;
}

/* tighter v-alert -- no leading icon, 4px vertical / 8px horizontal
   padding, 0.9rem body font. The font-size has to be on .v-alert__content
   (and !important) because Vuetify's own scope wins over :deep on the
   outer .v-alert selector. */
:deep(.v-alert) {
  padding: 4px 8px;
  min-height: 0;
  line-height: 1.2;
}
:deep(.v-alert),
:deep(.v-alert__content),
:deep(.v-alert .v-alert__content) {
  font-size: 0.9rem !important;
}
</style>
