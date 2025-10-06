<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="spigraph-popup">
    <template
      v-for="(field, index) of fieldList"
      :key="index"
    >
      <b-card
        :key="field.exp"
        v-if="index < popupInfo.depth"
        class="mb-2"
      >
        <b-card-title>
          {{ field.friendlyName }}
          <a
            class="pull-right cursor-pointer no-decoration"
            v-if="index === 0"
            @click="closeInfo"
          >
            <span class="fa fa-close" />
          </a>
        </b-card-title>
        <b-card-text>
          <arkime-session-field
            :key="field.exp"
            :field="field"
            :value="getPopupInfo(index).name"
            :expr="field.exp"
            :parse="true"
            :session-btn="true"
          />
        </b-card-text>
        <template #footer>
          <div
            class="d-flex justify-content-around text-center"
            style="line-height: 1;"
          >
            <div class="stat">
              {{ $t('spigraph.tableCount') }}
              <br>
              <b-badge>
                {{ commaString(getPopupInfo(index).size) }}
              </b-badge>
            </div>
            <div class="stat">
              Src IPs
              <br>
              <b-badge>
                {{ commaString(getPopupInfo(index).srcips) }}
              </b-badge>
            </div>
            <div class="stat">
              Dst IPs
              <br>
              <b-badge>
                {{ commaString(getPopupInfo(index).dstips) }}
              </b-badge>
            </div>
          </div>
        </template>
      </b-card>
    </template>
  </div>
</template>

<script>
import { commaString } from '@common/vueFilters.js';

export default {
  name: 'Popup',
  emits: ['closeInfo'],
  props: {
    fieldList: {
      type: Array,
      default: () => []
    },
    popupInfo: {
      type: Object,
      default: () => ({})
    }
  },
  methods: {
    commaString,
    closeInfo () {
      this.$emit('closeInfo');
    },
    getPopupInfo (index) {
      let info = this.popupInfo;
      const i = index + 1;
      while (info.parent) {
        if (i === info.depth) {
          if (info.data.sizeValue) {
            info.data.size = info.data.sizeValue;
          }
          return info.data;
        }
        info = info.parent;
      }
    }
  }
};
</script>

<style scoped>
.spigraph-popup .card {
  font-size: 0.9rem;
  box-shadow: 0px 5px 10px 0px black;
}

.spigraph-popup .card-title {
  font-size: 1rem;
  margin-bottom: -4px;
}
.spigraph-popup .card-footer {
  font-size: 0.8rem;
}

.stat {
  padding: 3px;
  border-radius: 4px;
  margin: 0 5px 0 5px;
  color: var(--color-foreground);
  border: 1px solid var(--color-primary);
}
</style>

<style>
.spigraph-popup > .card > .card-body,
.spigraph-popup > .card > .card-footer {
  padding: 0.2rem 0.3rem !important;
}
</style>
