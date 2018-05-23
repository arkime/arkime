<template>

  <div class="mb-1">

    <!-- typeahead input -->
    <div class="input-group input-group-sm">
      <span class="input-group-prepend cursor-help"
        v-b-tooltip.hover
        placement="bottomright"
        title="Search Expression">
        <span class="input-group-text">
          <span class="fa fa-search"></span>
        </span>
      </span>
      <input type="text"
        tabindex="1"
        v-model="expression"
        v-caret-pos="caretPos"
        v-focus-input="focusInput"
        placeholder="Search"
        @blur="onOffFocus"
        @input="debounceExprChange"
        @keyup.enter="enterClick"
        @keyup.esc.tab.enter.down.up.stop="keyup($event)"
        class="form-control search-control" />
      <span v-if="expression"
        @click="clear()"
        class="fa fa-close text-muted clear-input cursor-pointer">
      </span>
    </div> <!-- /typeahead input -->

    <!-- TODO fix tooltip placement issues -->
    <!-- results dropdown -->
    <div id="typeahead-results"
      ref="typeaheadResults"
      class="dropdown-menu typeahead-results"
      v-show="expression && results && results.length">
      <a v-for="(value, key) in results"
        :key="key"
        class="dropdown-item cursor-pointer"
        :class="{'active':key === activeIdx}"
        @click="addToQuery(value)"
        v-b-tooltip.hover
        placement="right"
        :title="value.help">
        <strong v-if="value.exp">{{ value.exp }}</strong>
        <strong v-if="!value.exp">{{ value }}</strong>
        <span v-if="value.friendlyName">- {{ value.friendlyName }}</span>
      </a>
    </div> <!-- /results dropdown -->

    <!-- error -->
    <div class="dropdown-menu typeahead-results"
      v-show="expression && loadingError">
      <a class="dropdown-item text-danger">
        <span class="fa fa-warning"></span>&nbsp;
        Error: {{ loadingError }}
      </a>
    </div> <!-- /error -->

    <!-- loading -->
    <div class="dropdown-menu typeahead-results"
      v-show="expression && loadingValues">
      <a class="dropdown-item">
        <span class="fa fa-spinner fa-spin"></span>&nbsp;
        Loading...
      </a>
    </div> <!-- /loading -->

  </div>

</template>

<script>
import FieldService from './FieldService';
import CaretPos from '../utils/CaretPos';
import FocusInput from '../utils/FocusInput';

let tokens;
let timeout;

const operations = ['==', '!=', '<', '<=', '>', '>='];

export default {
  name: 'ExpressionTypeahead',
  directives: { CaretPos, FocusInput },
  data: function () {
    return {
      activeIdx: -1,
      focusInput: true,
      results: [],
      fields: null,
      loadingError: '',
      loadingValues: false,
      caretPos: 0,
      cancellablePromise: null,
      resultsElement: null
    };
  },
  computed: {
    expression: {
      get: function () {
        return this.$store.state.expression;
      },
      set: function (newValue) {
        this.$store.commit('setExpression', newValue);
      }
    }
  },
  watch: {
    // watch for route update of expression
    '$route.query.expression': function (newVal, oldVal) {
      this.expression = newVal;

      // reset necessary vars
      this.results = null;
      this.focusInput = true;
      this.activeIdx = -1;

      // notify parent
      this.$emit('changeExpression');
    }
  },
  created: function () {
    if (this.$route.query.expression) {
      this.expression = this.$route.query.expression;
    }
    this.getFields();
  },
  mounted: function () {
    // set the results element for keyup event handler
    this.resultsElement = this.$refs.typeaheadResults;
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    clear: function () {
      this.expression = undefined;
    },
    /**
     * Fired when a value from the typeahead menu is selected
     * @param {Object} val The value to be added to the query
     */
    addToQuery: function (val) {
      let str = val;
      if (val.exp) { str = val.exp; }

      this.expression = this.rebuildQuery(this.expression, str);

      this.results = null;
      this.focusInput = true; // re-focus on input
      this.activeIdx = -1;
    },
    /* Fired when the search input is changed */
    debounceExprChange: function () {
      this.cancelPromise();

      if (timeout) { clearTimeout(timeout); }
      timeout = setTimeout(() => {
        this.changeExpression();
      }, 500);
    },
    enterClick: function () {
      // only apply the expression on enter click when not selecting
      // something inside the typeahead dropdown results list
      if (this.activeIdx < 0) { this.$emit('applyExpression'); }
    },
    /**
     * Watches for keyup events for escape, tab, enter, down, and up keys
     * Pressing the up and down arrows navigate the dropdown.
     * Pressing enter, adds the active item in the dropdown to the query.
     * Pressing escape, removes the typeahead results.
     * @param {Object} event The keydown event fired by the input
     */
    keyup: function (event) {
      let target;

      // always check for escape before anything else
      if (event.keyCode === 27) {
        // if there's a request in progress, cancel it
        this.cancelPromise();

        this.loadingValues = false;
        this.loadingError = false;
        this.results = null;
        this.activeIdx = -1;

        return;
      }

      // check for tab click when results are visible
      if (this.results && this.results.length && event.keyCode === 9) {
        event.preventDefault();

        // if there is no item in the results is selected, use the first one
        if (this.activeIdx < 0) { this.activeIdx = 0; }

        let result = this.results[this.activeIdx];
        if (result) { this.addToQuery(result); }

        this.cancelPromise();

        this.loadingValues = false;
        this.loadingError = false;
        this.results = null;
        this.activeIdx = -1;

        return;
      }

      // if there are no results, just check for enter click to remove typeahead
      if (!this.results || this.results.length === 0) {
        if (event.keyCode === 13) {
          this.cancelPromise();

          this.loadingValues = false;
          this.loadingError = false;
          this.results = null;
          this.activeIdx = -1;
        }

        return;
      }

      if (!this.activeIdx && this.activeIdx !== 0) { this.activeIdx = -1; }

      switch (event.keyCode) {
        case 40: // down arrow
          event.preventDefault();
          this.activeIdx = (this.activeIdx + 1) % this.results.length;
          target = this.resultsElement.querySelectorAll('a')[this.activeIdx];
          target.parentNode.scrollTop = target.offsetTop;
          break;
        case 38: // up arrow
          event.preventDefault();
          this.activeIdx = (this.activeIdx > 0 ? this.activeIdx : this.results.length) - 1;
          target = this.resultsElement.querySelectorAll('a')[this.activeIdx];
          target.parentNode.scrollTop = target.offsetTop;
          break;
        case 13: // enter
          if (this.activeIdx >= 0) {
            event.preventDefault();
            let result = this.results[this.activeIdx];
            if (result) { this.addToQuery(result); }
          }
          break;
      }
    },
    /* Removes typeahead results */
    onOffFocus: function () {
      setTimeout(() => {
        if (timeout) { clearTimeout(timeout); }

        this.cancelPromise();

        this.results = null;
        this.activeIdx = -1;
      }, 300);
    },
    /* helper functions ------------------------------------------ */
    /* Displays appropriate typeahead suggestions */
    changeExpression: function () {
      this.activeIdx = -1;
      this.results = null;
      this.focusInput = false;
      this.loadingValues = false;
      this.loadingError = false;

      // if the cursor is at a space
      let spaceCP = (this.caretPos > 0 &&
        this.caretPos === this.expression.length &&
        this.expression[this.caretPos - 1] === ' ');

      let end = this.caretPos;
      let endLen = this.expression.length;
      for (end; end < endLen; ++end) {
        if (this.expression[end] === ' ') {
          break;
        }
      }

      let currentInput = this.expression.substr(0, end);
      tokens = this.splitExpression(currentInput);

      // add the space to the tokens
      if (spaceCP) { tokens.push(' '); }

      let lastToken = tokens[tokens.length - 1];

      // display fields
      if (tokens.length <= 1) {
        this.results = this.findMatch(lastToken, this.fields);
        return;
      }

      // display operators (depending on field type)
      let token = tokens[tokens.length - 2];
      let field = this.fields[token];
      if (field) {
        if (field.type === 'integer') {
          // if at a space, show all operators
          if (tokens[tokens.length - 1] === ' ') {
            this.results = operations;
          } else {
            this.results = this.findMatch(lastToken, operations);
          }
        } else {
          this.results = this.findMatch(lastToken, ['!=', '==']);
        }

        return;
      }

      // save the operator token for possibly adding 'EXISTS!' result
      let operatorToken = token;

      token = tokens[tokens.length - 3];
      field = this.fields[token];

      if (!field) {
        if (/^[!<=>]/.test(token)) {
          // if at a space, show all operators
          if (tokens[tokens.length - 1] === ' ') {
            this.results = ['&&', '||'];
          } else {
            this.results = this.findMatch(lastToken, ['&&', '||']);
          }
        } else {
          this.results = this.findMatch(lastToken, this.fields);
        }

        return;
      }

      // regular expressions start with a forward slash
      // lists start with an open square bracket
      // don't issue query for these types of values
      if (/^(\/|\[)/.test(lastToken)) { return; }

      // display values
      // autocomplete country values
      if (/^(country)/.test(token)) {
        this.loadingValues = true;
        this.FieldService.getCountryCodes()
          .then((result) => {
            this.loadingValues = false;
            this.results = this.findMatch(lastToken, result);
            this.addExistsItem(lastToken, operatorToken);
          })
          .catch((error) => {
            this.loadingValues = false;
            this.loadingError = error;
          });

        return;
      }

      // autocomplete other values after 2 chars
      if (lastToken.trim().length >= 2) {
        let params = { // build parameters for getting value(s)
          autocomplete: true,
          field: field.dbField
        };

        if (this.$route.query.date) {
          params.date = this.$route.query.date;
        } else if (this.$route.query.startTime && this.$route.query.stopTime) {
          params.startTime = this.$route.query.startTime;
          params.stopTime = this.$route.query.stopTime;
        }

        if (field.type === 'ip') {
          params.expression = token + '==' + lastToken;
        } else {
          params.expression = token + '==' + lastToken + '*';
        }

        this.loadingValues = true;

        this.cancellablePromise = FieldService.getValues(params);

        this.cancellablePromise.promise
          .then((result) => {
            this.cancellablePromise = null;
            if (result) {
              this.loadingValues = false;
              this.loadingError = '';
              this.results = result;
              this.addExistsItem(lastToken, operatorToken);
            }
          })
          .catch((error) => {
            this.cancellablePromise = null;
            this.loadingValues = false;
            this.loadingError = error;
          });
      }
    },
    /**
     * Adds 'EXISTS!' result item to the typeahead options
     * @param {string} strToMatch The string to compare 'EXISTS!' to
     * @param {string} operator   The operator of the expression
     */
    addExistsItem: function (strToMatch, operator) {
      if (operator !== '==' && operator !== '!=') { return; }

      if ('EXISTS!'.match(new RegExp(strToMatch + '.*'))) {
        this.results.push('EXISTS!');
      }
    },
    /* aborts a pending promise */
    cancelPromise: function () {
      if (this.cancellablePromise) {
        this.cancellablePromise.source.cancel();
        this.cancellablePromise = null;
        this.loadingValues = false;
        this.loadingError = '';
      }
    },
    getFields: function () {
      FieldService.get()
        .then((result) => {
          this.fields = result;
          this.loadingError = '';
        })
        .catch((error) => {
          this.loadingError = error;
        });
    },
    /**
     * Finds matching items from an array or map of values
     * @param {string} strToMatch  The string to compare with the values
     * @param {Object} values      Map or Array of values to compare against
     */
    findMatch: function (strToMatch, values) {
      if (!strToMatch || strToMatch === '') { return null; }

      let results = [];
      let exact = false;

      for (let key in values) {
        if (values.hasOwnProperty(key)) {
          let str;
          let field = values[key];

          strToMatch = strToMatch.toLowerCase();

          if (field.exp) {
            str = field.exp.toLowerCase();
          } else {
            str = field.toLowerCase();
          }

          if (str === strToMatch) {
            exact = field;
          } else {
            let match = str.match(strToMatch);
            if (match) { results.push(field); }
          }
        }
      }

      // put the exact match at the top (the rest are in the order received)
      if (exact) { results.unshift(exact); }

      if (!results.length) { results = null; }

      return results;
    },
    /**
     * Builds the query by either appending a string onto it or replacing
     * the last token with a string and rebuilding the query string
     * @param {string} q   The query string to be appended to or rebuilt
     * @param {string} str The string to add to the query
     */
    rebuildQuery: function (q, str) {
      let result = '';
      let lastToken = tokens[tokens.length - 1];
      let allTokens = this.splitExpression(q);

      if (lastToken === ' ') {
        result = q += str + ' ';
      } else { // replace the last token and rebuild query
        let t, i;
        tokens[tokens.length - 1] = str;

        for (i = 0; i < tokens.length; ++i) {
          t = tokens[i];
          if (t === ' ') { break; }
          result += t + ' ';
        }

        if (allTokens.length > tokens.length) {
          // add the rest of the tokens
          for (i; i < allTokens.length; ++i) {
            t = allTokens[i];
            if (t === ' ') { break; }
            result += t + ' ';
          }
        }
      }

      return result;
    },
    /**
     * Splits a string into tokens
     * @param {string} input The string to be tokenized
     */
    splitExpression: function (input) {
      // replace spaces that are not enclosed in quotes
      input = input.replace(/ (?=([^"]*"[^"]*")*[^"]*$)/g, '');

      let output = [];
      let cur = '';

      for (let i = 0, ilen = input.length; i < ilen; i++) {
        if (/[)(]/.test(input[i])) {
          if (cur !== '') {
            output.push(cur);
          }
          output.push(input[i]);
          cur = '';
        } else if (cur === '') {
          cur += input[i];
        } else if (/[!&|=<>]/.test(cur) || cur === 'EXISTS' || cur === 'EXISTS!') {
          if ((/[&|=<>]/.test(input[i]) && cur !== 'EXISTS!') || (cur === 'EXISTS' && input[i] === '!')) {
            cur += input[i];
          } else {
            output.push(cur);
            cur = input[i];
          }
        } else if (/[!&|=<>]/.test(input[i])) {
          output.push(cur);
          cur = input[i];
        } else {
          cur += input[i];
        }
      }

      if (cur !== '') {
        output.push(cur);
      }

      return output;
    }
  },
  beforeDestroy: function () {
    this.cancelPromise();
    if (timeout) { clearTimeout(timeout); }
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
  margin-left: 30px;
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
