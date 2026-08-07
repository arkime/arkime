<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="spigraph-popup">
    <template
      v-for="(field, index) of fieldList"
      :key="index">
      <div
        :key="field.exp"
        v-if="index < popupInfo.depth"
        class="popup-card mb-2">
        <div class="popup-card-title">
          {{ field.friendlyName }}
          <a
            class="float-right cursor-pointer no-decoration"
            v-if="index === 0"
            @click="closeInfo">
            <v-icon icon="mdi-close" />
          </a>
        </div>
        <div class="popup-card-body">
          <arkime-session-field
            :key="field.exp"
            :field="field"
            :value="getPopupInfo(index).name"
            :expr="field.exp"
            :parse="true"
            :session-btn="true" />
        </div>
        <div class="popup-card-footer">
          <div
            class="d-flex justify-space-around text-center"
            style="line-height: 1;">
            <div
              v-for="stat in popupStats(index)"
              :key="stat.label"
              class="stat">
              {{ stat.label }}
              <br>
              <span class="stat-value">
                {{ stat.value }}
              </span>
            </div>
          </div>
        </div>
      </div>
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
    /* The 3 stat badges in the card footer share an identical
       structure -- collapse them into one v-for over a config list. */
    popupStats (index) {
      const info = this.getPopupInfo(index);
      return [
        { label: this.$t('spigraph.tableCount'), value: this.commaString(info.size) },
        { label: 'Src IPs', value: this.commaString(info.srcips) },
        { label: 'Dst IPs', value: this.commaString(info.dstips) }
      ];
    },
    getPopupInfo (index) {
      let info = this.popupInfo;
      const i = index + 1;

      // Handle simple case where depth matches and there's no complex parent structure
      if (i === info.depth && (!info.parent || !info.parent.parent)) {
        if (info.data.sizeValue) {
          info.data.size = info.data.sizeValue;
        }
        return info.data;
      }

      // Handle hierarchical structure
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
/* Replacement for b-card / b-card-title / b-card-text / footer slot.
   Bordered card with a tinted title and tighter padding for the
   spi-graph drill-down popup. */
.spigraph-popup .popup-card {
  font-size: 0.9rem;
  background-color: rgb(var(--v-theme-background));
  color: rgb(var(--v-theme-foreground));
  border: 1px solid rgb(var(--v-theme-neutral-light));
  border-radius: 4px;
  box-shadow: 0px 5px 10px 0px black;
}
.spigraph-popup .popup-card-title {
  font-size: 1rem;
  margin-bottom: -4px;
  padding: 0.2rem 0.3rem;
  font-weight: 500;
}
.spigraph-popup .popup-card-body {
  padding: 0.2rem 0.3rem;
}
.spigraph-popup .popup-card-footer {
  font-size: 0.8rem;
  padding: 0.2rem 0.3rem;
  border-top: 1px solid rgb(var(--v-theme-neutral-light));
}

.stat {
  padding: 3px;
  border-radius: 4px;
  margin: 0 5px 0 5px;
  color: rgb(var(--v-theme-foreground));
  border: 1px solid rgb(var(--v-theme-primary));
}
</style>
