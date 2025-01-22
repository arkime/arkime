<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="spigraph-popup">
    <template v-for="(field, index) of fieldList">
      <b-card
        :key="field.exp"
        v-if="index < popupInfo.depth"
        class="mb-2">
        <b-card-title>
          {{ field.friendlyName }}
          <a class="pull-right cursor-pointer no-decoration"
            v-if="index === 0"
            @click="closeInfo">
            <span class="fa fa-close"></span>
          </a>
        </b-card-title>
        <b-card-text>
          <arkime-session-field
            :key="field.exp"
            :field="field"
            :value="getPopupInfo(index).name"
            :expr="field.exp"
            :parse="true"
            :session-btn="true">
          </arkime-session-field>
        </b-card-text>
        <template #footer>
          <div class="d-flex justify-content-around text-center">
            <div class="stat">
              Count
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
import { commaString } from '@real_common/vueFilters.js';

export default {
  name: 'Popup',
  props: {
    fieldList: Array,
    popupInfo: Object
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

.spigraph-popup .card-body,
.spigraph-popup .card-footer {
  padding: 0.25rem;
}

.spigraph-popup .card-title {
  margin-bottom: -4px;
}

.stat {
  padding: 3px;
  border-radius: 4px;
  margin: 0 5px 0 5px;
  border: 1px solid var(--color-primary);
}
</style>
