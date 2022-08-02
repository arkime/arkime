<template>
  <b-card v-if="value">
    <div class="d-xl-flex mb-2">
      <div class="d-xl-flex flex-grow-1 flex-wrap mt-1">
        <h4 class="text-warning">
          {{ itype.toUpperCase() }}
        </h4>
        <cont3xt-field
            :value="value"
            class="align-self-center mr-1"
            :id="`${value}-${itype}`"
        />
        <slot name="after-field"></slot>

        <!--    unlabeled tidbits    -->
        <template v-for="(tidbit, index) in unlabeledTidbits">
          <integration-tidbit :tidbit="tidbit" :key="index"
                              :id="`${value}-tidbit-${index}`"/>
        </template><!--    /unlabeled tidbits    -->

      </div>
      <div class="d-flex align-self-center justify-content-end">
        <integration-btns
            :data="data"
            :itype="itype"
            :value="value"
        />
      </div>
    </div>

        <!--  labeled tidbits  -->
    <div v-for="(tidbit, index) in labeledTidbits" class="row ml-3" :key="index">
      <div class="col">
        <integration-tidbit :tidbit="tidbit" :id="`${value}-labeled-tidbit-${index}`"/>
      </div>
    </div><!--  /labeled tidbits  -->

    <span v-for="(child, index) in completeChildren" :key="index">
      <cont3xt-domain
          v-bind="child"
          v-if="child.itype === 'domain'"
      />
      <cont3xt-ip
          v-bind="child"
          v-else-if="child.itype === 'ip'"
      />
      <cont3xt-url
          v-bind="child"
          v-else-if="child.itype === 'url'"
      />
      <cont3xt-email
          v-bind="child"
          v-else-if="child.itype === 'email'"
      />
      <cont3xt-hash
          v-bind="child"
          v-else-if="child.itype === 'hash'"
      />
      <cont3xt-phone
          v-bind="child"
          v-else-if="child.itype === 'phone'"
      />
      <cont3xt-text
          v-bind="child"
          v-else-if="child.itype === 'text'"
      />
    </span>
    <slot name="after-children"></slot>
  </b-card>
</template>

<script>
import Cont3xtField from '@/utils/Field';
import IntegrationBtns from '@/components/integrations/IntegrationBtns';
import IntegrationTidbit from '@/components/integrations/IntegrationTidbit';

export default {
  name: 'ITypeBasis',
  components: {
    Cont3xtField,
    IntegrationBtns,
    IntegrationTidbit,
    // NOTE: need async import here because there's a circular dependency
    //       between ITypeBasis and the different implementation types
    // see: vuejs.org/v2/guide/components.html#Circular-References-Between-Components
    Cont3xtDomain: () => import('@/components/itypes/Domain'),
    Cont3xtEmail: () => import('@/components/itypes/Email'),
    Cont3xtIp: () => import('@/components/itypes/IP'),
    Cont3xtText: () => import('@/components/itypes/Text'),
    Cont3xtHash: () => import('@/components/itypes/Hash'),
    Cont3xtPhone: () => import('@/components/itypes/Phone'),
    Cont3xtUrl: () => import('@/components/itypes/URL')
  },
  props: {
    value: {
      type: String,
      required: true
    },
    tidbits: {
      type: Array,
      default () { return []; }
    },
    itype: {
      type: String,
      required: true
    },
    data: {
      type: Object,
      required: true
    },
    children: { // array of shape [{ itype: string, query: string }, ...]
      type: Array,
      default () { return []; }
    }
  },
  computed: {
    completeChildren () {
      return this.children.map(child => ({ data: this.data, ...child }));
    },
    labeledTidbits () {
      return this.tidbits.filter(tidbit => tidbit.label?.length);
    },
    unlabeledTidbits () {
      return this.tidbits.filter(tidbit => !tidbit.label?.length);
    }
  }
};
</script>

<style scoped>

</style>
