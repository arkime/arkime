<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="i18n-example p-4">
    <h3>{{ $t('common.settings') }}</h3>

    <!-- Example 1: Simple translation -->
    <div class="mb-3">
      <label class="form-label">{{ $t('search.expression') }}</label>
      <input type="text" class="form-control" :placeholder="$t('search.expression')" />
    </div>

    <!-- Example 2: Using useI18n composable -->
    <div class="mb-3">
      <button @click="performSearch" class="btn btn-primary">
        {{ searchButtonText }}
      </button>
      <span v-if="isLoading" class="ms-2">{{ loadingText }}</span>
    </div>

    <!-- Example 3: Pluralization (if you add plural forms) -->
    <div class="mb-3">
      <p>{{ $t('sessions.packets') }}: {{ packetCount }}</p>
    </div>

    <!-- Example 4: Translations in data attributes and computed properties -->
    <div class="mb-3">
      <select v-model="selectedTimeRange" class="form-select">
        <option value="">{{ $t('search.timeRange') }}</option>
        <option v-for="range in timeRangeOptions" :key="range.value" :value="range.value">
          {{ range.label }}
        </option>
      </select>
    </div>

    <!-- Example 5: Dynamic translation with variables -->
    <div class="alert alert-info">
      {{ welcomeMessage }}
    </div>

    <!-- Language switcher integration -->
    <div class="mb-3">
      <LanguageSwitcher />
    </div>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useI18n } from 'vue-i18n';
import LanguageSwitcher from './LanguageSwitcher.vue';

// Using the Composition API approach
const { t, locale } = useI18n();

// Reactive data
const isLoading = ref(false);
const packetCount = ref(1234);
const selectedTimeRange = ref('');

// Computed properties using translations
const searchButtonText = computed(() => t('search.searchSessions'));
const loadingText = computed(() => t('common.loading'));

// Dynamic welcome message based on current locale
const welcomeMessage = computed(() => {
  const messages = {
    en: `Welcome to Arkime! Current language: ${locale.value}`,
    es: `¡Bienvenido a Arkime! Idioma actual: ${locale.value}`,
    fr: `Bienvenue dans Arkime! Langue actuelle: ${locale.value}`,
    de: `Willkommen bei Arkime! Aktuelle Sprache: ${locale.value}`,
    ja: `Arkimeへようこそ！現在の言語: ${locale.value}`,
    zh: `欢迎使用Arkime！当前语言: ${locale.value}`
  };
  return messages[locale.value] || messages.en;
});

// Options array with translations
const timeRangeOptions = computed(() => [
  { value: '1h', label: t('common.loading') + ' 1 hour' },
  { value: '1d', label: t('common.loading') + ' 1 day' },
  { value: '1w', label: t('common.loading') + ' 1 week' }
]);

// Methods using translations
const performSearch = () => {
  isLoading.value = true;
  // Simulate API call
  setTimeout(() => {
    isLoading.value = false;
    alert(t('search.searchSessions') + ' completed!');
  }, 2000);
};
</script>

<script>
// Options API example (alternative approach)
export default {
  name: 'I18nExample',
  components: {
    LanguageSwitcher
  },
  data() {
    return {
      // You can also use this.$t in Options API
    };
  },
  computed: {
    // Example of using translations in Options API
    helpText() {
      return this.$t('common.help');
    }
  },
  methods: {
    showErrorMessage() {
      // Using $t in methods
      alert(this.$t('errors.networkError'));
    }
  }
};
</script>

<style scoped>
.i18n-example {
  max-width: 600px;
  margin: 0 auto;
}

.form-label {
  font-weight: 600;
  margin-bottom: 0.5rem;
}

.alert {
  border-radius: 0.375rem;
}
</style>
