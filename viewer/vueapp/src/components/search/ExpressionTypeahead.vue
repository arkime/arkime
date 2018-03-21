<template>

  <div class="mb-1">

    <div class="input-group input-group-sm">
      <span class="input-group-prepend cursor-help"
        v-b-tooltip.hover
        placement="bottomright"
        title="Search Expression">
        <span class="input-group-text">
          <span class="fa fa-search"></span>
        </span>
      </span>
      <!-- <input caret-pos="$ctrl.caretPos" focus-input="$ctrl.focusInput"
        ng-model="$root.expression" ng-trim="false"
        ng-blur="$ctrl.onOffFocus()"
        ng-keydown="$ctrl.keydown($event);"
        ng-change="$ctrl.debounceExprChange()"
        type="text" placeholder="Search" tabindex="1"
        class="form-control search-control" />
      <span ng-click="$ctrl.clear()" ng-if="$root.expression"
        class="fa fa-close text-muted clear-input cursor-pointer">
      </span> -->
      <!-- TODO make this a typeahead! -->
      <input type="text"
        tabindex="1"
        placeholder="Search"
        class="form-control search-control" />
      <span @click="clear()"
        class="fa fa-close text-muted clear-input cursor-pointer">
      </span>
    </div>

  </div>

</template>

<script>
export default {
  name: 'ExpressionTypeahead',
  data: function () {
    return {};
  },
  created: function () {
  },
  methods: {
    clear: function () {
      // TODO
    }
  }
};
</script>

<style scoped>
.input-group {
  flex-wrap: none;
  width: auto;
}

.clear-input {
  z-index: 3;
  position: absolute;
  right: 10px;
  top: 8px;
}

.typeahead-results {
  top: initial;
  left: initial;
  display: block;
  overflow-y: auto;
  overflow-x: hidden;
  max-height: 500px;
}

/* make sure corners are rounded even
 * when the clear input button is present */
input.search-control {
  border-radius: 0 3px 3px 0 !important;
}

@media screen and (max-height: 600px) {
  .typeahead-results {
    max-height: 250px;
  }
}
</style>
