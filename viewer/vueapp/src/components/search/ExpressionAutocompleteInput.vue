<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <textarea
    v-if="textarea"
    ref="inputEl"
    :rows="rows"
    :placeholder="placeholder"
    :disabled="disabled"
    :value="modelValue"
    class="form-control form-control-sm"
    @input="onInput"
    @keydown="onKeydown"
    @click="updateCaretPos"
    @blur="onBlur" />
  <input
    v-else
    ref="inputEl"
    type="text"
    :placeholder="placeholder"
    :disabled="disabled"
    :value="modelValue"
    class="form-control form-control-sm"
    @input="onInput"
    @keydown="onKeydown"
    @click="updateCaretPos"
    @blur="onBlur">
  <Teleport to="body">
    <div
      v-show="modelValue && results && results.length"
      ref="dropdownEl"
      class="dropdown-menu expression-autocomplete-dropdown"
      :style="dropdownStyle">
      <template v-if="autocompletingField">
        <template
          v-for="(value, key) in fieldHistoryResults"
          :key="key + 'history'">
          <a
            class="dropdown-item cursor-pointer"
            :class="{'active': key === activeIdx, 'last-history-item': key === fieldHistoryResults.length - 1}"
            @click="addToQuery(value)">
            <span class="fa fa-history" />&nbsp;
            <strong v-if="value.exp">{{ value.exp }}</strong>
            <strong v-if="!value.exp">{{ value }}</strong>
            <span v-if="value.friendlyName">- {{ value.friendlyName }}</span>
            <span
              class="fa fa-close pull-right mt-1"
              :title="`Remove ${value.exp} from your field history`"
              @click.stop.prevent="removeFromFieldHistory(value)" />
          </a>
        </template>
      </template>
      <template
        v-for="(value, key) in results"
        :key="value + 'item'">
        <a
          class="dropdown-item cursor-pointer"
          :title="value.help"
          :class="{'active': key + fieldHistoryResults.length === activeIdx}"
          @click="addToQuery(value)">
          <strong v-if="value.exp">{{ value.exp }}</strong>
          <strong v-if="!value.exp">{{ value }}</strong>
          <span v-if="value.friendlyName">- {{ value.friendlyName }}</span>
        </a>
      </template>
    </div>
    <div
      class="dropdown-menu expression-autocomplete-dropdown"
      :style="dropdownStyle"
      v-show="modelValue && loadingError">
      <a class="dropdown-item text-danger">
        <span class="fa fa-warning" />&nbsp;
        Error: {{ loadingError }}
      </a>
    </div>
    <div
      class="dropdown-menu expression-autocomplete-dropdown"
      :style="dropdownStyle"
      v-show="modelValue && loadingValues">
      <a class="dropdown-item">
        <span class="fa fa-spinner fa-spin" />&nbsp;
        Loading...
      </a>
    </div>
  </Teleport>
</template>

<script setup>
import { ref, computed, watch, nextTick, onBeforeUnmount } from 'vue';
import { useStore } from 'vuex';
import { useRoute } from 'vue-router';
import FieldService from './FieldService';
import UserService from '../users/UserService';

const props = defineProps({
  modelValue: {
    type: String,
    default: ''
  },
  textarea: {
    type: Boolean,
    default: false
  },
  rows: {
    type: [Number, String],
    default: 3
  },
  placeholder: {
    type: String,
    default: ''
  },
  disabled: {
    type: Boolean,
    default: false
  }
});

const emit = defineEmits(['update:modelValue', 'apply']);

const store = useStore();
const route = useRoute();

const inputEl = ref(null);
const dropdownEl = ref(null);

const activeIdx = ref(-1);
const results = ref(null);
const loadingError = ref('');
const loadingValues = ref(false);
const caretPos = ref(0);
const cancellablePromise = ref(null);
const fieldHistoryResults = ref([]);
const lastTokenWasField = ref(false);
const autocompletingField = ref(false);

let tokens = [];
let debounceTimeout = null;

const operations = ['==', '!=', '<', '<=', '>', '>='];

// dropdown positioning
const dropdownPos = ref({ top: 0, left: 0, width: 0 });

const dropdownStyle = computed(() => ({
  position: 'fixed',
  display: 'block',
  top: `${dropdownPos.value.top}px`,
  left: `${dropdownPos.value.left}px`,
  width: `${dropdownPos.value.width}px`,
  zIndex: 1060,
  maxHeight: '300px',
  overflowY: 'auto',
  overflowX: 'hidden'
}));

// store access
const fields = computed(() => store.state.fieldsArr);
const views = computed(() => store.state.views);
const fieldHistory = computed(() => store.state.fieldhistory);

// --- Utility functions ported from ExpressionTypeahead ---

function splitExpression (input) {
  input = input.replace(/ (?=([^"]*"[^"]*")*[^"]*$)/g, '');

  const output = [];
  let cur = '';

  for (let i = 0, ilen = input.length; i < ilen; i++) {
    if (/[)(]/.test(input[i])) {
      if (cur !== '') { output.push(cur); }
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

  if (cur !== '') { output.push(cur); }

  return output;
}

function findMatch (strToMatch, values) {
  if (!strToMatch || strToMatch === '') { return null; }

  let matchResults = [];
  let exact = false;

  for (const key in values) {
    let str;
    const field = values[key];

    strToMatch = strToMatch.toLowerCase();

    if (field.exp) {
      str = field.exp.toLowerCase();
    } else {
      str = field.toLowerCase();
    }

    if (str === strToMatch) {
      exact = field;
    } else {
      const match = str.match(strToMatch);
      if (match) { matchResults.push(field); }
    }
  }

  if (exact) { matchResults.unshift(exact); }

  if (!matchResults.length) { matchResults = null; }

  return matchResults;
}

function rebuildQuery (q, str) {
  let result = '';
  let lastToken = tokens[tokens.length - 1];
  const allTokens = splitExpression(q);
  let replacingToken = lastToken;

  if (lastToken === ' ') {
    replacingToken = null;
    result = q += str + ' ';
  } else {
    let t, i;

    const isArray = /^(\[)/.test(lastToken);
    if (isArray) {
      let pos = caretPos.value;

      if (props.modelValue[pos + 1] !== ',' && props.modelValue[pos + 1] !== ']') {
        const subExpr = tokens.join(' ');
        const tokenCaretPos = caretPos.value - (subExpr.length - lastToken.length);
        if (tokenCaretPos !== lastToken.length - 1) {
          lastToken = lastToken.slice(0, tokenCaretPos + 1) + ',' + lastToken.slice(tokenCaretPos + 1);
        }
      }

      let hasEndBracket = false;
      if (/(])$/.test(lastToken)) {
        hasEndBracket = true;
        lastToken = lastToken.substring(0, lastToken.length - 1);
      }
      const split = lastToken.split(',');
      split[0] = split[0].substring(1);

      const queryChars = [];
      for (pos; pos >= 0; pos--) {
        const char = props.modelValue[pos];
        if (char === ',' || char === '[') { break; }
        queryChars.push(char);
      }

      replacingToken = queryChars.reverse().join('');

      for (i = 0; i < split.length; ++i) {
        if (split[i] === replacingToken) {
          split[i] = str;
          break;
        }
      }

      str = `[${split.join(',')}`;
      if (hasEndBracket) { str += ']'; }
    }

    tokens[tokens.length - 1] = str;

    for (i = 0; i < tokens.length; ++i) {
      t = tokens[i];
      if (t === ' ') { break; }
      result += t + ' ';
    }

    if (allTokens.length > tokens.length) {
      for (i; i < allTokens.length; ++i) {
        t = allTokens[i];
        if (t === ' ') { break; }
        result += t + ' ';
      }
    }
  }

  return { expression: result, replacing: replacingToken };
}

function addExistsItem (strToMatch, operator) {
  if (operator !== '==' && operator !== '!=') { return; }

  try {
    if ('EXISTS!'.match(new RegExp(strToMatch + '.*'))) {
      results.value.push('EXISTS!');
    }
  } catch (error) {}
}

function cancelPromise () {
  if (cancellablePromise.value) {
    cancellablePromise.value.controller.abort('Request cancelled');
    cancellablePromise.value = null;
    loadingValues.value = false;
    loadingError.value = '';
  }
}

function addFieldToHistory (field) {
  let found = false;
  if (!field) { return found; }

  let index = 0;
  for (const historyField of fieldHistory.value) {
    if (historyField.exp === field.exp) {
      found = true;
      break;
    }
    index++;
  }

  if (found) { fieldHistory.value.splice(index, 1); }

  fieldHistory.value.unshift(field);

  if (fieldHistory.value.length > 30) {
    fieldHistory.value.splice(fieldHistory.value.length - 1, 1);
  }

  UserService.saveState({ fields: fieldHistory.value }, 'fieldHistory');

  return found;
}

function removeFromFieldHistory (field) {
  let index = 0;
  for (const historyField of fieldHistory.value) {
    if (historyField.exp === field.exp) { break; }
    index++;
  }

  fieldHistory.value.splice(index, 1);

  index = 0;
  for (const historyField of fieldHistoryResults.value) {
    if (historyField.exp === field.exp) { break; }
    index++;
  }

  fieldHistoryResults.value.splice(index, 1);

  UserService.saveState({ fields: fieldHistory.value }, 'fieldHistory');
}

function updateFieldHistoryResults (strToMatch) {
  lastTokenWasField.value = true;
  autocompletingField.value = true;
  fieldHistoryResults.value = findMatch(strToMatch, fieldHistory.value) || [];
}

async function changeExpression () {
  activeIdx.value = -1;
  results.value = null;
  fieldHistoryResults.value = [];
  loadingValues.value = false;
  loadingError.value = '';
  lastTokenWasField.value = false;
  autocompletingField.value = false;

  const expr = props.modelValue;
  if (!expr) { return; }

  const spaceCP = (caretPos.value > 0 && expr[caretPos.value] === ' ');

  let end = caretPos.value;
  const endLen = expr.length;
  for (end; end < endLen; ++end) {
    if (expr[end] === ' ') { break; }
  }

  const currentInput = expr.substr(0, end);
  tokens = splitExpression(currentInput);

  if (spaceCP) { tokens.push(' '); }

  let lastToken = tokens[tokens.length - 1];

  if (tokens.length <= 1) {
    results.value = findMatch(lastToken, fields.value);
    updateFieldHistoryResults(lastToken);
    return;
  }

  let token = tokens[tokens.length - 2];
  let field = FieldService.getField(token, true);

  if (field) { addFieldToHistory(field); }

  if (field) {
    if (field.type === 'integer') {
      if (tokens[tokens.length - 1] === ' ') {
        results.value = operations;
      } else {
        results.value = findMatch(lastToken, operations);
      }
    } else {
      results.value = findMatch(lastToken, ['!=', '==']);
    }
    return;
  }

  const operatorToken = token;

  token = tokens[tokens.length - 3];
  field = FieldService.getField(token, true);

  if (!field) {
    if (/^[!<=>]/.test(token)) {
      if (tokens[tokens.length - 1] === ' ') {
        results.value = ['&&', '||'];
      } else {
        results.value = findMatch(lastToken, ['&&', '||']);
      }
    } else {
      results.value = findMatch(lastToken, fields.value);
      updateFieldHistoryResults(lastToken);
    }
    return;
  }

  if (field.type === 'viewand') {
    const viewsObj = Object.fromEntries(Object.keys(views.value).map((v) => [v, v]));
    results.value = findMatch(lastToken, viewsObj);
  }

  if (/^(\$)/.test(lastToken)) {
    loadingValues.value = true;
    let url = 'api/shortcuts?fieldFormat=true&map=true';
    if (field && field.type) {
      url += `&fieldType=${field.type}`;
    }

    try {
      const response = await FieldService.getShortcuts(url);
      loadingValues.value = false;
      const escapedToken = lastToken.replaceAll('$', '\\$');
      results.value = findMatch(escapedToken, response.data);
    } catch (error) {
      loadingValues.value = false;
      loadingError.value = error.text || String(error);
    }
    return;
  }

  if (field.noFacet || field.regex || field.type.match(/textfield/)) { return; }

  let isValueList = false;
  if (/^(\[)/.test(lastToken)) {
    isValueList = true;
    lastToken = lastToken.substring(1);
  }

  if (/^(\/|\[)/.test(lastToken)) { return; }

  if (/^(country)/.test(token)) {
    loadingValues.value = true;
    FieldService.getCountryCodes().then((result) => {
      loadingValues.value = false;
      results.value = findMatch(lastToken, result);
      addExistsItem(lastToken, operatorToken);
    }).catch((error) => {
      loadingValues.value = false;
      loadingError.value = error;
    });
    return;
  }

  if (isValueList && /(,)/.test(lastToken)) {
    let pos = caretPos.value;
    const queryChars = [];
    for (pos; pos >= 0; pos--) {
      const char = expr[pos];
      if (char === ',' || char === '[') { break; }
      queryChars.push(char);
    }
    lastToken = queryChars.reverse().join('');

    if (/(])$/.test(lastToken)) {
      lastToken = lastToken.substring(0, lastToken.length - 1);
    }
  }

  if (lastToken.trim().length >= 2 && lastToken[0] !== '"') {
    const params = {
      autocomplete: lastToken,
      field: field.dbField
    };

    if (route.query.date) {
      params.date = route.query.date;
    } else if (route.query.startTime !== undefined && route.query.stopTime !== undefined) {
      params.startTime = route.query.startTime;
      params.stopTime = route.query.stopTime;
    }

    if (field.type === 'integer') {
      // no expression filter for integer fields
    } else if (field.type === 'ip') {
      params.expression = token + '==' + lastToken;
    } else {
      params.expression = token + '==' + lastToken + '*';
    }

    loadingValues.value = true;

    try {
      const { controller, fetcher } = FieldService.getValues(params);
      cancellablePromise.value = { controller };
      const result = await fetcher;
      cancellablePromise.value = null;
      loadingValues.value = false;
      loadingError.value = '';
      results.value = result;
      addExistsItem(lastToken, operatorToken);
    } catch (error) {
      cancellablePromise.value = null;
      loadingValues.value = false;
      loadingError.value = error.message || error;
    }
  }
}

// --- Dropdown positioning ---

function updateDropdownPos () {
  if (!inputEl.value) { return; }
  const rect = inputEl.value.getBoundingClientRect();
  dropdownPos.value = {
    top: rect.bottom,
    left: rect.left,
    width: rect.width
  };
}

// --- Event handlers ---

function updateCaretPos () {
  if (inputEl.value && 'selectionStart' in inputEl.value) {
    caretPos.value = inputEl.value.selectionStart;
  }
}

function onInput (e) {
  updateCaretPos();
  emit('update:modelValue', e.target.value);

  cancelPromise();

  if (debounceTimeout) { clearTimeout(debounceTimeout); }
  debounceTimeout = setTimeout(() => {
    changeExpression();
    nextTick(updateDropdownPos);
  }, 500);
}

function addToQuery (val) {
  let str = val;
  if (val.exp) { str = val.exp; }

  const { expression, replacing } = rebuildQuery(props.modelValue, str);
  emit('update:modelValue', expression);

  if (lastTokenWasField.value) { addFieldToHistory(val); }

  results.value = null;
  fieldHistoryResults.value = [];
  activeIdx.value = -1;

  nextTick(() => {
    if (inputEl.value) {
      if (replacing) {
        const newCaretPos = caretPos.value + 1 + (str.length - replacing.length);
        inputEl.value.setSelectionRange(newCaretPos, newCaretPos);
      }
      inputEl.value.focus();
    }
  });
}

function onKeydown (e) {
  // track caret position
  if (inputEl.value && 'selectionStart' in inputEl.value) {
    caretPos.value = inputEl.value.selectionStart;
  }

  if (e.key === 'Escape') {
    cancelPromise();
    loadingValues.value = false;
    loadingError.value = '';

    if (results.value && results.value.length) {
      // clear results but stop propagation so parent modals don't close
      results.value = null;
      fieldHistoryResults.value = [];
      activeIdx.value = -1;
      e.stopPropagation();
      e.preventDefault();
    }
    // if no results, let the event propagate (e.g. to close parent modal)
    return;
  }

  if (e.key === 'Tab') {
    if (results.value && results.value.length) {
      e.preventDefault();
      e.stopPropagation();
      if (activeIdx.value < 0) { activeIdx.value = 0; }
      const result = results.value[activeIdx.value];
      if (result) { addToQuery(result); }
      cancelPromise();
      loadingValues.value = false;
      loadingError.value = '';
      results.value = null;
      fieldHistoryResults.value = [];
      activeIdx.value = -1;
    }
    return;
  }

  if (e.key === 'Enter') {
    if (!props.textarea) {
      e.preventDefault();
    }

    // if results are showing and an item is selected, select it
    if (results.value && results.value.length && activeIdx.value >= 0) {
      e.preventDefault();
      e.stopPropagation();
      let result;
      if (activeIdx.value < fieldHistoryResults.value.length) {
        result = fieldHistoryResults.value[activeIdx.value];
      } else {
        result = results.value[activeIdx.value - fieldHistoryResults.value.length];
      }
      if (result) {
        caretPos.value--;
        addToQuery(result);
      }
      return;
    }

    // no active selection — emit apply and clear results
    if (!props.textarea || (e.ctrlKey || e.metaKey)) {
      e.preventDefault();
      e.stopPropagation();
      cancelPromise();
      if (debounceTimeout) { clearTimeout(debounceTimeout); }
      results.value = null;
      fieldHistoryResults.value = [];
      emit('apply');
    }
    return;
  }

  if (e.key === 'ArrowDown') {
    if (results.value && results.value.length) {
      e.preventDefault();
      e.stopPropagation();
      activeIdx.value = (activeIdx.value + 1) % (fieldHistoryResults.value.length + results.value.length);
      scrollToActive();
    }
    return;
  }

  if (e.key === 'ArrowUp') {
    if (results.value && results.value.length) {
      e.preventDefault();
      e.stopPropagation();
      const total = fieldHistoryResults.value.length + results.value.length;
      activeIdx.value = (activeIdx.value > 0 ? activeIdx.value : total) - 1;
      scrollToActive();
    }
  }
}

function scrollToActive () {
  nextTick(() => {
    if (dropdownEl.value) {
      const items = dropdownEl.value.querySelectorAll('a');
      const target = items[activeIdx.value];
      if (target) {
        target.scrollIntoView({ block: 'nearest' });
      }
    }
  });
}

function onBlur () {
  setTimeout(() => {
    if (debounceTimeout) { clearTimeout(debounceTimeout); }
    cancelPromise();
    results.value = null;
    fieldHistoryResults.value = [];
    activeIdx.value = -1;
  }, 300);
}

// watch for results changes to update dropdown position
watch(results, () => {
  nextTick(updateDropdownPos);
});

// expose focus method for parent components
function focusInput () {
  inputEl.value?.focus();
}

defineExpose({ focus: focusInput });

onBeforeUnmount(() => {
  cancelPromise();
  if (debounceTimeout) { clearTimeout(debounceTimeout); }
});
</script>

<style>
.expression-autocomplete-dropdown {
  overflow-y: auto;
  overflow-x: hidden;
}
.expression-autocomplete-dropdown a.last-history-item {
  border-bottom: 1px solid var(--color-gray);
}
</style>
