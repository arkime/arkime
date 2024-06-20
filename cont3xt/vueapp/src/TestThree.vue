<template>
  <div>
    <p>Times clicked: {{ count }}</p>
    <button @click="increment">increment</button>

    <div v-for="n of [...(Array(count).fill(0).map((_, i) => i))]" :key="n">
      <span>what {{n}}</span>
    </div>
  </div>
</template>

<script>
export default {
  data: () => ({
    count: 0
  }),

  methods: {
    increment () {
      this.count++;
    }
  }
};
</script>
