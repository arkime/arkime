<template>

  <div class="mb-1"
    v-on-clickaway="onOffFocus">

    <!-- typeahead input -->
    <div class="input-group input-group-sm">
      <span class="input-group-prepend input-group-prepend-fw cursor-help"
        v-b-tooltip.hover
        placement="bottomright"
        title="Search Expression">
        <span class="input-group-text input-group-text-fw">
          <span v-if="!shiftKeyHold"
            class="fa fa-search fa-fw">
          </span>
          <span v-else
            class="query-shortcut">
            Q
          </span>
        </span>
      </span>
      <input type="text"
        tabindex="1"
        v-model="expression"
        v-caret-pos="caretPos"
        v-focus-input="focusInput"
        placeholder="Search"
        @input="debounceExprChange"
        @keyup.enter="enterClick"
        @keyup.esc.tab.enter.down.up.stop="keyup($event)"
        class="form-control search-control"
      />
      <span class="input-group-append">
        <button type="button"
          @click="clear"
          :disabled="!expression"
          class="btn btn-outline-secondary btn-clear-input">
          <span class="fa fa-close">
          </span>
        </button>
      </span>
    </div> <!-- /typeahead input -->

    <!-- results dropdown -->
    <div id="typeahead-results"
      ref="typeaheadResults"
      class="dropdown-menu typeahead-results"
      v-show="expression && results && results.length">
      <template v-if="autocompletingField">
        <template v-for="(value, key) in fieldHistoryResults">
          <a :id="key+'history'"
            :key="key+'history'"
            class="dropdown-item cursor-pointer"
            :class="{'active':key === activeIdx,'last-history-item':key === fieldHistoryResults.length-1}"
            @click="addToQuery(value)">
            <span class="fa fa-history"></span>&nbsp;
            <strong v-if="value.exp">{{ value.exp }}</strong>
            <strong v-if="!value.exp">{{ value }}</strong>
            <span v-if="value.friendlyName">- {{ value.friendlyName }}</span>
            <span class="fa fa-close pull-right mt-1"
              @click.stop.prevent="removeFromFieldHistory(value)">
            </span>
          </a>
          <b-tooltip v-if="value.help"
            :key="key+'historytooltip'"
            :target="key+'history'"
            placement="right"
            boundary="window">
            {{ value.help.substring(0, 100) }}
            <span v-if="value.help.length > 100">
              ...
            </span>
          </b-tooltip>
        </template>
      </template>
      <template v-for="(value, key) in results">
        <a :id="key+'item'"
          :key="key+'item'"
          class="dropdown-item cursor-pointer"
          :class="{'active':key+fieldHistoryResults.length === activeIdx}"
          @click="addToQuery(value)">
          <strong v-if="value.exp">{{ value.exp }}</strong>
          <strong v-if="!value.exp">{{ value }}</strong>
          <span v-if="value.friendlyName">- {{ value.friendlyName }}</span>
        </a>
        <b-tooltip v-if="value.help"
          :key="key+'tooltip'"
          :target="key+'item'"
          placement="right"
          boundary="window">
          {{ value.help.substring(0, 100) }}
          <span v-if="value.help.length > 100">
            ...
          </span>
        </b-tooltip>
      </template>
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
import UserService from '../users/UserService';
import FieldService from './FieldService';
import CaretPos from '../utils/CaretPos';
import FocusInput from '../utils/FocusInput';
import { mixin as clickaway } from 'vue-clickaway';

let tokens;
let timeout;

const operations = ['==', '!=', '<', '<=', '>', '>='];

export default {
  name: 'ExpressionTypeahead',
  mixins: [ clickaway ],
  directives: { CaretPos, FocusInput },
  data: function () {
    return {
      activeIdx: -1,
      results: [],
      fields: null,
      loadingError: '',
      loadingValues: false,
      caretPos: 0,
      cancellablePromise: null,
      resultsElement: null,
      // field history vars
      fieldHistory: [],
      fieldHistoryResults: [],
      lastTokenWasField: false,
      autocompletingField: false
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
    },
    focusInput: {
      get: function () {
        return this.$store.state.focusSearch;
      },
      set: function (newValue) {
        this.$store.commit('setFocusSearch', newValue);
      }
    },
    shiftKeyHold: function () {
      return this.$store.state.shiftKeyHold;
    }
  },
  watch: {
    // watch for route update of expression
    '$route.query.expression': function (newVal, oldVal) {
      this.expression = newVal;

      // reset necessary vars
      this.results = null;
      this.fieldHistoryResults = [];
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

    UserService.getState('fieldHistory')
      .then((response) => {
        this.fieldHistory = response.data.fields || [];
      });
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

      if (this.lastTokenWasField) { // add field to history
        this.addFieldToHistory(val);
      }

      this.results = null;
      this.fieldHistoryResults = [];
      this.focusInput = true; // re-focus on input
      this.activeIdx = -1;

      setTimeout(() => { // unfocus input for further re-focusing
        this.focusInput = false;
      }, 1000);
    },
    /* Fired when the search input is changed */
    debounceExprChange: function () {
      this.cancelPromise();

      if (timeout) { clearTimeout(timeout); }
      timeout = setTimeout(() => {
        this.changeExpression();
      }, 500);
    },
    /* fired when enter is clicked from the expression typeahead input */
    enterClick: function () {
      // if the activeIdx >= 0, an item in the dropdown is selected
      if (this.activeIdx >= 0) { return; }
      // only apply the expression and clear the results on enter click when
      // not selecting something inside the typeahead dropdown results list
      this.$emit('applyExpression');
      // clear the timeout for the expression input change so it
      // doesn't update the results which opens the results dropdown
      if (timeout) { clearTimeout(timeout); }
      // cancel any queries to get values for fields so that the
      // dropdown isn't populated with more results
      this.cancelPromise();
      // clear out the results to close the dropdown
      this.results = null;
      this.fieldHistoryResults = [];
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

        // if there's no results blur the input
        if (!this.results || !this.results.length) {
          event.target.blur();
          this.focusInput = false;
        }

        this.loadingValues = false;
        this.loadingError = false;
        this.results = null;
        this.fieldHistoryResults = [];
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
        this.fieldHistoryResults = [];
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
          this.fieldHistoryResults = [];
          this.activeIdx = -1;
        }

        return;
      }

      if (!this.activeIdx && this.activeIdx !== 0) { this.activeIdx = -1; }

      switch (event.keyCode) {
        case 40: // down arrow
          event.preventDefault();
          this.activeIdx = (this.activeIdx + 1) % (this.fieldHistoryResults.length + this.results.length);
          target = this.resultsElement.querySelectorAll('a')[this.activeIdx];
          if (target && target.parentNode) {
            target.parentNode.scrollTop = target.offsetTop;
          }
          break;
        case 38: // up arrow
          event.preventDefault();
          this.activeIdx = (this.activeIdx > 0 ? this.activeIdx : (this.fieldHistoryResults.length + this.results.length)) - 1;
          target = this.resultsElement.querySelectorAll('a')[this.activeIdx];
          if (target && target.parentNode) {
            target.parentNode.scrollTop = target.offsetTop;
          }
          break;
        case 13: // enter
          if (this.activeIdx >= 0) {
            event.preventDefault();
            let result;
            if (this.activeIdx < this.fieldHistoryResults.length) {
              result = this.fieldHistoryResults[this.activeIdx];
            } else {
              result = this.results[this.activeIdx - this.fieldHistoryResults.length];
            }
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
        this.fieldHistoryResults = [];
        this.activeIdx = -1;
        this.focusInput = false;
      }, 300);
    },
    /**
     * Removes an item to the field history (and results)
     * @param {object} field The field to remove from the history
     */
    removeFromFieldHistory: function (field) {
      let index = 0;
      for (let historyField of this.fieldHistory) {
        if (historyField.exp === field.exp) {
          break;
        }
        index++;
      }

      // remove the item from the history
      this.fieldHistory.splice(index, 1);

      // find it in the field history results (displayed in the typeahead)
      index = 0;
      for (let historyField of this.fieldHistoryResults) {
        if (historyField.exp === field.exp) {
          break;
        }
        index++;
      }

      // remove the item from the field history resutls
      this.fieldHistoryResults.splice(index, 1);

      // save the field history for the user
      UserService.saveState({ fields: this.fieldHistory }, 'fieldHistory');
    },
    /* helper functions ------------------------------------------ */
    /**
     * Adds an item to the beginning of the field history
     * @param {object} field The field to add to the history
     */
    addFieldToHistory: function (field) {
      let found = false;

      if (!field) { return found; }

      let index = 0;
      for (let historyField of this.fieldHistory) {
        if (historyField.exp === field.exp) {
          found = true;
          break;
        }
        index++;
      }

      if (found) { // if the field was found, remove it
        this.fieldHistory.splice(index, 1);
      }

      // add the field to the beginning of the list
      this.fieldHistory.unshift(field);

      // if the list is larger than 30 items
      if (this.fieldHistory.length > 30) {
        // remove the last item in the history
        this.fieldHistory.splice(this.fieldHistory.length - 1, 1);
      }

      // save the field history for the user
      UserService.saveState({ fields: this.fieldHistory }, 'fieldHistory');

      return found;
    },
    /**
     * Filters the field history results and sets varaibles so that the view
     * and other functions know that a field is being searched in the typeahead
     * @param {string} strToMatch The string to compare the field history to
     */
    updateFieldHistoryResults: function (strToMatch) {
      this.lastTokenWasField = true;
      this.autocompletingField = true;
      this.fieldHistoryResults = this.findMatch(strToMatch, this.fieldHistory) || [];
    },
    /* Displays appropriate typeahead suggestions */
    changeExpression: function () {
      this.activeIdx = -1;
      this.results = null;
      this.fieldHistoryResults = [];
      this.loadingValues = false;
      this.loadingError = false;
      this.lastTokenWasField = false;
      this.autocompletingField = false;

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
        this.updateFieldHistoryResults(lastToken);
        return;
      }

      // display operators (depending on field type)
      let token = tokens[tokens.length - 2];
      let field = this.fields[token];

      if (field) { // add field to the history
        this.addFieldToHistory(field);
      }

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
          this.updateFieldHistoryResults(lastToken);
        }

        return;
      }

      // Don't try and autocomplete these fields
      if (field.noFacet || field.regex || field.type.match(/textfield/)) { return; }

      // regular expressions start with a forward slash
      // lists start with an open square bracket
      // don't issue query for these types of values
      if (/^(\/|\[)/.test(lastToken)) { return; }

      // display values
      // autocomplete country values
      if (/^(country)/.test(token)) {
        this.loadingValues = true;
        FieldService.getCountryCodes()
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

      // autocomplete variables
      if (/^(\$)/.test(lastToken)) {
        this.loadingValues = true;
        let url = 'lookups?fieldFormat=true&map=true';
        if (field && field.type) {
          url += `&fieldType=${field.type}`;
        }
        this.$http.get(url)
          .then((response) => {
            this.loadingValues = false;
            let escapedToken = lastToken.replace('$', '\\$');
            this.results = this.findMatch(escapedToken, response.data);
          }, (error) => {
            this.loadingValues = false;
            this.loadingError = error.text || error;
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
        } else if (this.$route.query.startTime !== undefined &&
          this.$route.query.stopTime !== undefined) {
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
            this.loadingError = error.message || error;
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

      try {
        if ('EXISTS!'.match(new RegExp(strToMatch + '.*'))) {
          this.results.push('EXISTS!');
        }
      } catch (error) {}
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

.typeahead-results {
  top: initial;
  left: initial;
  display: block;
  overflow-y: auto;
  overflow-x: hidden;
  max-height: 500px;
  margin-left: 35px;
}

.typeahead-results a.last-history-item {
  border-bottom: 1px solid var(--color-gray);
}

@media screen and (max-height: 600px) {
  .typeahead-results {
    max-height: 250px;
  }
}
</style>
