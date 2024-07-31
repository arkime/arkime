<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <cont3xt-card class="mb-2">
    <h5 class="text-warning mb-3"
      v-if="card && card.title">
      {{ card.title.replace('%{query}', indicator.query) }}
      <div class="float-right mt-1">
        <template v-if="filteredSearchUrls && filteredSearchUrls.length > 0">
          <template v-if="filteredSearchUrls.length === 1">
            <v-btn
              size="small"
              class="ml-1"
              target="_blank"
              variant="outlined"
              color="primary"
              v-if="filteredSearchUrls[0]"
              :href="filteredSearchUrls[0].url.replace('%{query}', indicator.query)"
              v-tooltip="filteredSearchUrls[0].name.replace('%{query}', indicator.query)">
              <span class="fa fa-external-link fa-fw"></span>
            </v-btn>
          </template>
          <template v-else>
            <b-dropdown
              right
              size="sm"
              variant="outline-primary"
              v-tooltip="`Pivot your search into ${source}`">
              <template #button-content>
                <span class="fa fa-external-link fa-fw"></span>
              </template>
              <template v-for="searchUrl in card.searchUrls">
                <b-dropdown-item
                  target="_blank"
                  :key="searchUrl.name"
                  v-if="searchUrl.itypes.includes(indicator.itype)"
                  :href="searchUrl.url.replace('%{query}', indicator.query)">
                  {{ searchUrl.name.replace('%{query}', indicator.query) }}
                </b-dropdown-item>
              </template>
            </b-dropdown>
          </template>
        </template>
        <v-btn
          size="small"
          class="ml-1"
          tabindex="-1"
          @click="copy"
          v-tooltip="'Copy as raw JSON'"
          title="Copy as raw JSON"
          variant="outlined"
          color="success">
          <span class="fa fa-copy fa-fw" />
        </v-btn>
        <v-btn
          size="small"
          class="ml-1"
          tabindex="-1"
          @click="download"
          v-tooltip="'Download as raw JSON'"
          title="Download as raw JSON"
          variant="outlined"
          color="success">
          <span class="fa fa-download fa-fw" />
        </v-btn>
        <v-btn
          size="small"
          class="ml-1"
          tabindex="-1"
          @click="refresh"
          v-tooltip="`Queried ${moment(integrationData._cont3xt.createTime, 'from')}\n${dateString(integrationData._cont3xt.createTime)}`"
          variant="outlined"
          color="info"
          :title="`Queried ${moment(integrationData._cont3xt.createTime, 'from')}\n${dateString(integrationData._cont3xt.createTime)}`">
          <span class="fa fa-refresh fa-fw" />
        </v-btn>
      </div>
    </h5>
    <!-- error with data -->
    <v-alert
      v-if="!!error"
      color="error"
      class="flex-grow-1">
      <span class="pr-2">
        <span class="fa fa-exclamation-triangle fa-fw fa-3x" />
      </span>
      <div class="display-inline-block">
        <strong>Error:</strong>
        <br>
        {{ error }}
      </div>
    </v-alert> <!-- error with data -->
    <!-- no template -->
    <v-alert
      v-if="!card"
      color="warning">
      <span class="pr-2">
        <span class="fa fa-exclamation-triangle fa-fw fa-3x" />
      </span>
      <div class="display-inline-block">
        Missing information to render the data.
        <br>
        Please make sure your integration has a "card" attribute.
      </div>
    </v-alert> <!-- no template -->
    <!-- no data -->
    <template v-if="Object.keys(integrationData).length === 1 && integrationData._cont3xt.createTime">
      <h5 class="display-4 text-center mt-4 mb-4 text-muted">
        <span class="fa fa-folder-open" />
        <br>
        No data
      </h5>
    </template> <!-- /no data -->
    <template v-else> <!-- data -->
      <!-- card template -->
      <template v-if="card && card.fields">
        <div
          v-for="field in card.fields"
          :key="field.label">
          <integration-value
            :field="field"
            v-if="integrationData"
            :data="integrationData"
          />
        </div>
      </template> <!-- /card template -->
      <!-- raw -->
      <v-expansion-panels class="mt-2">
        <v-expansion-panel color="cont3xt-card">
          <template #title>
            <strong class="text-warning">
              raw
            </strong>
          </template>
          <template #text>
            <pre class="text-info overflow-x-auto">{{ integrationData }}</pre>
          </template>
        </v-expansion-panel>
      </v-expansion-panels>
      <!-- /raw -->
    </template> <!-- /data -->
  </cont3xt-card>
</template>

<script>
import { mapGetters } from 'vuex';
import moment from 'moment-timezone';
import { dateString } from '@/utils/filters.js';
import Cont3xtCard from '@/utils/Cont3xtCard.vue';

import Cont3xtService from '@/components/services/Cont3xtService';
import IntegrationValue from '@/components/integrations/IntegrationValue.vue';
import { Cont3xtIndicatorProp, getIntegrationData } from '@/utils/cont3xtUtil';
import { clipboardCopyText } from '@/utils/clipboardCopyText';

// NOTE: IntegrationCard displays IntegrationValues AND IntegrationTables
// IntegrationTables can ALSO display IntegrationValues, so:
// IntegrationCard -> IntegrationValue
// IntegrationCard -> IntegrationValue -> IntegrationTable -> IntegrationValue
export default {
  name: 'IntegrationCard',
  components: { IntegrationValue, Cont3xtCard },
  props: {
    source: { // the name of the integration to display data from
      type: String,
      required: true
    },
    indicator: Cont3xtIndicatorProp // the indicator to display data for
  },
  data () {
    return {
      error: '',
      loading: false
    };
  },
  computed: {
    ...mapGetters(['getIntegrations', 'getResults']),
    card () {
      if (!this.getIntegrations[this.source]) { return {}; }
      return this.getIntegrations[this.source].card;
    },
    integrationData () {
      return getIntegrationData(this.getResults, this.indicator, this.source);
    },
    filteredSearchUrls () {
      return this.card?.searchUrls?.filter(url => url.itypes.includes(this.indicator.itype));
    }
  },
  methods: {
    dateString,
    moment,
    refresh () {
      // display loading overload (parent update hides overlay)
      this.$store.commit('SET_RENDERING_CARD', true);
      Cont3xtService.refresh({ indicator: this.indicator, source: this.source }).then((response) => {
        // update the results in the parent so subsequent clicks on this
        // integration value's button has the updated results
        this.$emit('update-results', response);
      }).catch((err) => {
        this.error = err;
      });
    },
    copy () {
      this.error = '';

      if (!this.integrationData) {
        this.error = 'No raw data to copy!';
        return;
      }

      clipboardCopyText(JSON.stringify(this.integrationData, false, 2));
    },
    download () {
      this.error = '';

      if (!this.integrationData) {
        this.error = 'No raw data to copy!';
        return;
      }

      const a = document.createElement('a');
      const file = new Blob([JSON.stringify(this.integrationData, false, 2)], { type: 'application/json' });
      a.href = URL.createObjectURL(file);
      a.download = `${new Date().toISOString()}_${this.source.replaceAll(' ', '_')}_${this.indicator.query}.json`;
      a.click();
      URL.revokeObjectURL(a.href);
    }
  },
  mounted () { // card data is rendered (typically on first load)
    this.$nextTick(() => {
      this.$store.commit('SET_RENDERING_CARD', false);
    });
  },
  updated () { // card data is re-rendered (eg. via refresh)
    this.$nextTick(() => {
      this.$store.commit('SET_RENDERING_CARD', false);
    });
  }
};
</script>
