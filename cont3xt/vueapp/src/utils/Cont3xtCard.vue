<template>
  <v-card
    class="cont3xt-card pa-1"
    :elevation="5"
    :ripple="false">
    <slot />
  </v-card>
</template>

<style scoped>
.cont3xt-card {
  background-color: rgb(var(--v-theme-surface-card));
  border: 1px solid rgb(var(--v-theme-surface-card-border));
}
</style>
