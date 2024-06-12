<script setup>
import TobyTest from '@/components/links/TobyTest.vue';
import BadOldComp from './BadOldComp.vue';
import Vue from 'vue';
import TestingYou from '@common/TestingYou.vue';
console.log('hello, world!');

const { ref } = Vue;

setInterval(() => {
  console.log('check it out', Vue.$constants, Vue.prototype.$constants);
}, 1000);

const isDisabledButton = ref(false);
</script>

<template>
  <div>
    Hello, World!
    <toby-test/>
    <BadOldComp/>
    <TestingYou/>
    <b-button
      @click="isDisabledButton = !isDisabledButton"
      :disabled="isDisabledButton"
      >
      Hello {{ isDisabledButton }}
      </b-button>
    <b-alert
      variant="danger"
      :show="isDisabledButton"
      >
      Oh no!
    </b-alert>
  </div>
</template>
