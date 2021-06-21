<template>
  <div>
    <drag-list
      :list="list"
      @remove="remove"
      @reorder="reorder"
    />
  </div>
</template>

<script>
export default {
  data () {
    return {
      list: [
        { friendlyName: 'test0' },
        { friendlyName: 'test1' },
        { friendlyName: 'test2' },
        { friendlyName: 'test3' },
        { friendlyName: 'test4' }
      ]
    };
  },
  methods: {
    remove (index) {
      this.list.splice(index, 1);
    },
    reorder (list) {
      this.list = list;
    }
  }
};
</script>
