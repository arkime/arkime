/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

import { createI18n } from 'vue-i18n';
import { fetchWrapper } from '@common/fetchWrapper.js';

/**
 * Dynamically load all locale files from the backend API
 * This allows hot-reloading of locale files without rebuilding the app
 * @param {string} apiUrl - The API URL to fetch locales from (e.g., 'api/locales' or 'parliament/api/locales')
 */
async function loadLocales(apiUrl) {
  const messages = {};

  try {
    // Get all locale files at once from the backend
    const localesData = await fetchWrapper({ url: apiUrl });
    if (!localesData.success || !localesData.locales) {
      throw new Error('Invalid locales response format');
    }

    // Add all loaded locales to messages
    Object.keys(localesData.locales).forEach(localeCode => {
      const localeData = localesData.locales[localeCode];

      // Validate that the locale has proper structure
      if (localeData.__meta && localeData.__meta.code && localeData.__meta.name) {
        messages[localeCode] = localeData;
      } else {
        console.warn(`Invalid locale data structure for ${localeCode}`);
      }
    });

  } catch (error) {
    console.error('Failed to load locales from backend:', error);

    // Fallback: provide a minimal English locale if everything fails
    messages.en = {
      __meta: { code: 'en', name: 'English', countryCode: 'US' },
      common: { loading: 'Loading...', error: 'Error', search: 'Search' }
    };
  }

  return messages;
}

/**
 * Create and configure an i18n instance with dynamically loaded locales
 * @param {string} apiUrl - The API URL to fetch locales from
 * @returns {Object} Configured i18n instance
 */
export async function createI18nInstance(apiUrl) {
  // Create a placeholder i18n instance that will be configured after locale loading
  const i18n = createI18n({
    locale: 'en', // default locale
    fallbackLocale: 'en', // fallback locale when translation is missing
    legacy: false, // use Composition API (Vue 3) - required to avoid deprecation warning
    globalInjection: true, // enable global $t function
    silentTranslationWarn: true, // suppress translation warnings in production
    silentFallbackWarn: true, // suppress fallback warnings in production
    escapeParameterHtml: true, // escape HTML in translations to prevent XSS
    messages: {
      en: { loading: 'Loading...' } // temporary placeholder
    }
  });

  try {
    // Load all available locales
    const messages = await loadLocales(apiUrl);

    // Configure i18n with loaded messages
    Object.keys(messages).forEach(locale => {
      i18n.global.setLocaleMessage(locale, messages[locale]);
    });

    // Set default locale if English is available, otherwise use the first available locale
    const availableLocales = Object.keys(messages);
    if (availableLocales.includes('en')) {
      i18n.global.locale.value = 'en';
    } else if (availableLocales.length > 0) {
      i18n.global.locale.value = availableLocales[0];
    }
  } catch (error) {
    console.error('Failed to load locales:', error);
    // Continue with default configuration if locale loading fails
  }

  return i18n;
}
