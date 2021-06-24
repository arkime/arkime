<template>
  <div>
    <input
      v-caret-pos
      type="text"
      v-model="text"
      placeholder="text"
    />
    caret position is {{ caretPos }}
  </div>
</template>

<script>
export default {
  data () {
    return {
      text: '',
      caretPos: 0
    };
  }
};
</script>
