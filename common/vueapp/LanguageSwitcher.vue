<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <BDropdown
    no-caret
    size="xs"
    variant="light"
    id="language-dropdown"
    class="ms-2">
    <template #button-content>
      <span class="flag-icon">{{ currentLanguageFlag }}</span>
    </template>
    <BDropdownItem
      class="language-item"
      v-for="lang in availableLanguages"
      :key="lang.code"
      :active="lang.code === currentLocale"
      @click="changeLanguage(lang.code)">
      <span class="flag-icon me-2">{{ lang.flag }}</span>
      {{ lang.name }}
    </BDropdownItem>
  </BDropdown>
  <BTooltip target="language-dropdown" placement="left">
    {{ currentLanguageLabel }}
  </BTooltip>
</template>

<script setup>
import { computed } from 'vue';
import { useI18n } from 'vue-i18n';
import { countryCodeEmoji } from 'country-code-emoji';

const { locale } = useI18n();

// Dynamically generate available languages from loaded locales
const availableLanguages = computed(() => {
  const languages = [];

  try {
    // Get the global i18n instance from window or through the composable
    const i18nInstance = useI18n();

    // Access the global messages - need to get the actual value, not the reactive ref
    const globalI18n = i18nInstance.global || i18nInstance;
    const messages = globalI18n.messages?.value || globalI18n.messages || {};
    const locales = Object.keys(messages);


    for (const localeCode of locales) {
      try {
        const localeMessage = messages[localeCode];
        const meta = localeMessage?.__meta;

        if (meta && meta.code && meta.name) {
          const language = {
            code: meta.code,
            name: meta.name,
            flag: meta.customFlag || (meta.countryCode ? countryCodeEmoji(meta.countryCode) : '🌐')
          };
          languages.push(language);
        }
      } catch (error) {
        console.warn(`Failed to get metadata for locale ${localeCode}:`, error);
      }
    }
  } catch (error) {
    console.warn('Failed to load available locales:', error);
  }

  // Sort languages alphabetically by name, but keep English first and Pig Latin last
  return languages.sort((a, b) => {
    if (a.code === 'en') return -1;
    if (b.code === 'en') return 1;
    if (a.code === 'x-pl') return 1;
    if (b.code === 'x-pl') return -1;
    return a.name.localeCompare(b.name);
  });
});

// Current locale
const currentLocale = computed(() => locale.value);

// Current language info
const currentLanguage = computed(() => {
  const found = availableLanguages.value.find(lang => lang.code === currentLocale.value);
  if (found) return found;

  // Fallback when languages are still loading
  if (availableLanguages.value.length > 0) {
    return availableLanguages.value[0];
  }

  // Ultimate fallback
  return { code: 'en', name: 'English', flag: '🇺🇸' };
});

const currentLanguageLabel = computed(() => currentLanguage.value?.name || 'Loading...');
const currentLanguageFlag = computed(() => currentLanguage.value?.flag || '🌐');

// Change language and persist preference
const changeLanguage = (langCode) => {
  locale.value = langCode;

  // Store preference in localStorage
  localStorage.setItem('arkime-language', langCode);

  // Set HTML lang attribute for accessibility
  document.documentElement.lang = langCode;
};

// Detect browser's default language
const detectBrowserLanguage = () => {
  // Get browser language (e.g., 'en-US', 'es-ES', 'fr-FR')
  const browserLang = navigator.language || navigator.languages?.[0] || 'en';

  // Extract the base language code (e.g., 'en' from 'en-US')
  const baseLanguageCode = browserLang.split('-')[0].toLowerCase();

  // Check if we support this language
  const availableLangs = availableLanguages.value || [];
  const supportedLanguage = availableLangs.find(lang => lang.code === baseLanguageCode);

  return supportedLanguage ? baseLanguageCode : 'en'; // fallback to English
};

// Initialize language from localStorage, browser detection, or fallback to English
const initializeLanguage = () => {
  // Wait for languages to be loaded before initializing
  const availableLangs = availableLanguages.value || [];
  if (availableLangs.length === 0) {
    // Languages not loaded yet, try again in next tick
    setTimeout(initializeLanguage, 10);
    return;
  }

  // 1. First check localStorage for saved preference
  const savedLanguage = localStorage.getItem('arkime-language');
  if (savedLanguage && availableLangs.some(lang => lang.code === savedLanguage)) {
    changeLanguage(savedLanguage);
    return;
  }

  // 2. If no saved preference, detect browser language
  const browserLanguage = detectBrowserLanguage();
  changeLanguage(browserLanguage);
};

// Initialize on mount
initializeLanguage();
</script>

<style scoped>
.flag-icon {
  font-size: 1rem;
}
.language-item {
  min-height: 28px;
  max-height: 28px;
}
</style>
