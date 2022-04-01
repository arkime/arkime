<template>
  <span>
    <template v-if="field.join">
      {{ arrayData.join(field.join || ', ') }}
    </template>
    <template v-else>
      <div :key="index"
        v-for="index in (Math.max(arrayLen, 0))">
        {{ arrayData[index - 1] }}
      </div>
      <div class="d-flex justify-content-between"
        v-if="arrayData.length > arrayLen || arrayLen > size">
        <a
          @click="showLess"
          class="btn btn-link btn-xs"
          :class="{'disabled':arrayLen <= size}">
          show less...
        </a>
        <a
          @click="showAll"
          class="btn btn-link btn-xs"
          :class="{'disabled':arrayLen >= arrayData.length}">
          show ALL
        </a>
        <a
          @click="showMore"
          class="btn btn-link btn-xs"
          :class="{'disabled':arrayLen >= arrayData.length}">
          show more...
        </a>
      </div>
    </template>
  </span>
</template>

<script>
export default {
  name: 'IntegrationArray',
  props: {
    field: { // the field for which to display the array value
      type: Object,
      required: true
    },
    arrayData: { // the data to display the array
      type: Array,
      require: true
    },
    size: { // the rows of data to display initially and increment or
      type: Number, // decrement thereafter (by clicking more/less)
      default: 50
    }
  },
  data () {
    return {
      arrayLen: Math.min(this.arrayData.length, this.size)
    };
  },
  methods: {
    showMore () {
      this.arrayLen = Math.min(this.arrayLen + this.size, this.arrayData.length);
    },
    showLess () {
      this.arrayLen = Math.max(this.arrayLen - this.size, this.size);
    },
    showAll () {
      this.$store.commit('SET_RENDERING_ARRAY', true);
      setTimeout(() => { // need settimeout for rendering to take effect
        this.arrayLen = this.arrayData.length;
      }, 100);
    }
  },
  updated () { // data is rendered
    this.$nextTick(() => {
      this.$store.commit('SET_RENDERING_ARRAY', false);
    });
  }
};
</script>
