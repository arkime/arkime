<template>
  <div>Badness</div>
</template>
<script>
export default {
  name: 'BadOldComp',
  mounted () {
    setInterval(() => {
      console.log('badness', this.$constants, this, this.prototype, this.$store);
    }, 1000);
  }
};
</script>
