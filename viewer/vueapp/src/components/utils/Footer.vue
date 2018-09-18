<template>

  <div class="footer">
    <p>
      <small>
        Moloch v{{molochVersion}} |
        <a href="https://molo.ch">molo.ch</a>
        <span v-if="responseTime">
          | {{ responseTime | commaString }}ms
        </span>
      </small>
    </p>
  </div>

</template>

<script>
export default {
  name: 'MolochFooter',
  data: function () {
    return {
      molochVersion: this.$constants.MOLOCH_VERSION
    };
  },
  computed: {
    responseTime: function () {
      return this.$store.state.responseTime;
    }
  }
};
</script>

<style scoped>
.footer {
  text-align: center;
  margin-top: var(--px-md);
  color     : var(--color-gray-dark);
  border-top: 1px solid var(--color-gray-light);
}
</style>
