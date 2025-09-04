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
    class="language-switcher ms-2">
    <template #button-content>
      <span class="flag-icon">{{ currentLanguageFlag }}</span>
    </template>
    <BDropdownItem
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

const { locale, availableLocales } = useI18n();

// Available languages with their display information
const availableLanguages = [
  { code: 'en', name: 'English', flag: countryCodeEmoji('US') },
  { code: 'es', name: 'Español', flag: countryCodeEmoji('ES') },
  { code: 'fr', name: 'Français', flag: countryCodeEmoji('FR') },
  { code: 'de', name: 'Deutsch', flag: countryCodeEmoji('DE') },
  { code: 'ja', name: '日本語', flag: countryCodeEmoji('JP') },
  { code: 'ko', name: '한국어', flag: countryCodeEmoji('KR') },
  { code: 'zh', name: '中文', flag: countryCodeEmoji('CN') }
];

// Current locale
const currentLocale = computed(() => locale.value);

// Current language info
const currentLanguage = computed(() => {
  return availableLanguages.find(lang => lang.code === currentLocale.value) || availableLanguages[0];
});

const currentLanguageLabel = computed(() => currentLanguage.value.name);
const currentLanguageFlag = computed(() => currentLanguage.value.flag);

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
  const supportedLanguage = availableLanguages.find(lang => lang.code === baseLanguageCode);

  return supportedLanguage ? baseLanguageCode : 'en'; // fallback to English
};

// Initialize language from localStorage, browser detection, or fallback to English
const initializeLanguage = () => {
  // 1. First check localStorage for saved preference
  const savedLanguage = localStorage.getItem('arkime-language');
  if (savedLanguage && availableLanguages.some(lang => lang.code === savedLanguage)) {
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
</style>
