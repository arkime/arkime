# Arkime Internationalization Guide

This guide explains how to use and contribute to internationalization (i18n) in the Arkime viewer application using [Vue I18n v11](https://github.com/intlify/vue-i18n).

> **Contributing to translations?** See the [Contributing Guide](CONTRIBUTING.md#internationalization-i18n-globe_with_meridians) for information on how to add new languages or improve existing translations.

## 🚀 Quick Start

Vue I18n is already configured and ready to use! The setup includes:

- **Smart locale detection**: localStorage → browser language → fallback to English
- **7 supported languages**: English, Spanish, French, German, Japanese, Korean, Chinese
- **Composition API**: Modern Vue 3 approach with `useI18n()`
- **Global injection**: Use `$t()` in templates
- **Language persistence**: User preferences saved in localStorage
- **Shared components**: LanguageSwitcher and translation files available across all Arkime applications

## 📝 Using Translations in Components

### 1. Template Usage (Global `$t` function)

```vue
<template>
  <div>
    <!-- Simple translation -->
    <h1>{{ $t('navigation.sessions') }}</h1>

    <!-- In attributes -->
    <input :placeholder="$t('search.expression')" />

    <!-- In directives -->
    <button :title="$t('common.search')">Search</button>
  </div>
</template>
```

### 2. Composition API Usage

```vue
<script setup>
import { useI18n } from 'vue-i18n';

const { t, locale } = useI18n();

// Use in computed properties
const buttonText = computed(() => t('common.save'));

// Use in methods
const showMessage = () => {
  alert(t('errors.networkError'));
};

// Access current locale
console.log('Current language:', locale.value);
</script>
```

### 3. Options API Usage

```vue
<script>
export default {
  computed: {
    title() {
      return this.$t('sessions.title');
    }
  },
  methods: {
    handleError() {
      console.error(this.$t('errors.loadingFailed'));
    }
  }
};
</script>
```

## 🔧 Adding New Translations

### 1. Add to Translation Files

Update all locale files in `common/vueapp/locales/`:

```json
// en.json
{
  "newFeature": {
    "title": "New Feature",
    "description": "This is a new feature"
  }
}

// es.json
{
  "newFeature": {
    "title": "Nueva Funcionalidad",
    "description": "Esta es una nueva funcionalidad"
  }
}
```

### 2. Use in Components

```vue
<template>
  <div>
    <h2>{{ $t('newFeature.title') }}</h2>
    <p>{{ $t('newFeature.description') }}</p>
  </div>
</template>
```

## 🌐 Language Switching

### Automatic Language Detection

The LanguageSwitcher component automatically detects and sets the best language using this priority order:

1. **Saved preference** (localStorage): If user previously selected a language
2. **Browser language**: Detects browser's default language (`navigator.language`)
   - Handles locale variants (e.g., `en-US` → `en`, `es-MX` → `es`)
   - Only sets if we support the base language code
3. **Fallback to English**: If no saved preference and browser language not supported

### Using the Language Switcher Component

The LanguageSwitcher component uses the `country-code-emoji` package to generate flag emojis dynamically based on country codes.

```vue
<template>
  <div>
    <!-- Add anywhere in your template -->
    <LanguageSwitcher />
  </div>
</template>

<script setup>
import LanguageSwitcher from '@common/LanguageSwitcher.vue';
</script>
```

### Custom Language Selector with Flags

If you need to create your own language selector, you can use the `country-code-emoji` package:

```vue
<script setup>
import { countryCodeEmoji } from 'country-code-emoji';

const languages = [
  { code: 'en', name: 'English', flag: countryCodeEmoji('US') },
  { code: 'ko', name: '한국어', flag: countryCodeEmoji('KR') }
];
</script>
```

### Programmatic Language Switching

```vue
<script setup>
import { useI18n } from 'vue-i18n';

const { locale } = useI18n();

const switchToSpanish = () => {
  locale.value = 'es';
  localStorage.setItem('arkime-language', 'es');
  document.documentElement.lang = 'es';
};
</script>
```

### Browser Language Detection Implementation

The browser language detection follows this logic:

```javascript
const detectBrowserLanguage = () => {
  // Get browser language (e.g., 'en-US', 'es-ES', 'fr-FR')
  const browserLang = navigator.language || navigator.languages?.[0] || 'en';

  // Extract the base language code (e.g., 'en' from 'en-US')
  const baseLanguageCode = browserLang.split('-')[0].toLowerCase();

  // Check if we support this language
  const supportedLanguage = availableLanguages.find(lang => lang.code === baseLanguageCode);

  return supportedLanguage ? baseLanguageCode : 'en'; // fallback to English
};
```

**Examples of browser language detection:**
- Browser language `en-US` → detects `en` (English)
- Browser language `es-MX` → detects `es` (Spanish)
- Browser language `pt-BR` → fallback to `en` (Portuguese not supported)
- Browser language `ja-JP` → detects `ja` (Japanese)

## 🔄 Using i18n in Other Arkime Applications

The internationalization system is designed to be shared across all Arkime applications (viewer, cont3xt, parliament, wiseService).

### Setting up i18n in Other Apps

1. **Import shared translation files**:
```javascript
import { createI18n } from 'vue-i18n';
import english from '@common/locales/en.json';
import spanish from '@common/locales/es.json';
// ... other languages

const i18n = createI18n({
  messages: { en: english, es: spanish, /* ... */ }
});
```

2. **Use the shared LanguageSwitcher component**:
```vue
<template>
  <div>
    <LanguageSwitcher />
  </div>
</template>

<script setup>
import LanguageSwitcher from '@common/LanguageSwitcher.vue';
</script>
```

3. **Use translations in your components**:
```vue
<template>
  <h1>{{ $t('navigation.stats') }}</h1>
  <button>{{ $t('common.search') }}</button>
</template>
```

### Application-Specific Translations

If an application needs specific translations not shared across all apps:

1. **Create app-specific locale files** (e.g., `cont3xt-specific.json`)
2. **Merge with common translations**:
```javascript
import commonEnglish from '@common/locales/en.json';
import cont3xtEnglish from './locales/cont3xt-en.json';

const englishMessages = { ...commonEnglish, ...cont3xtEnglish };
```

## 📚 Translation Key Organization

Our translations are organized hierarchically:

```
common.*        - Universal UI elements (save, cancel, etc.)
navigation.*    - Menu items and navigation
sessions.*      - Session-related terms
search.*        - Search functionality
stats.*         - Statistics and metrics
errors.*        - Error messages
```

### Examples:

```javascript
$t('common.search')           // "Search"
$t('navigation.sessions')     // "Sessions"
$t('sessions.startTime')      // "Start Time"
$t('search.expression')       // "Search Expression"
$t('stats.captureStats')      // "Capture Statistics"
$t('errors.loadingFailed')    // "Failed to load data"
```

## 🎯 Best Practices

### 1. Always Use Translation Keys

❌ **Don't do this:**
```vue
<button>Search Sessions</button>
```

✅ **Do this:**
```vue
<button>{{ $t('search.searchSessions') }}</button>
```

### 2. Keep Keys Descriptive

❌ **Don't do this:**
```javascript
$t('btn1')  // unclear
$t('text')  // too generic
```

✅ **Do this:**
```javascript
$t('sessions.exportButton')
$t('search.timeRangeLabel')
```

### 3. Group Related Translations

```json
{
  "sessions": {
    "title": "Sessions",
    "export": "Export Sessions",
    "filter": "Filter Sessions",
    "columns": {
      "startTime": "Start Time",
      "endTime": "End Time",
      "protocol": "Protocol"
    }
  }
}
```

### 4. Handle Missing Translations Gracefully

Vue I18n will automatically fall back to English if a translation is missing.

## 🔍 Advanced Usage

### 1. Pluralization (when needed)

```json
{
  "sessions": {
    "count": "no sessions | {count} session | {count} sessions"
  }
}
```

```vue
<template>
  <p>{{ $t('sessions.count', sessionCount) }}</p>
</template>
```

### 2. Variable Interpolation

```json
{
  "welcome": "Welcome {username} to Arkime!"
}
```

```vue
<template>
  <h1>{{ $t('welcome', { username: user.name }) }}</h1>
</template>
```

### 3. Date/Time Formatting

```vue
<script setup>
import { useI18n } from 'vue-i18n';

const { d, n } = useI18n();

// Format dates according to locale
const formattedDate = d(new Date(), 'short');

// Format numbers according to locale
const formattedNumber = n(1234.56, 'currency');
</script>
```

## 🐛 Troubleshooting

### Translation Not Showing?

1. **Check the key exists** in all locale files
2. **Verify the path** is correct (`sessions.title` not `session.title`)
3. **Check console** for i18n warnings
4. **Ensure i18n is imported** in main.js

### Language Not Persisting?

1. **Check localStorage** permissions
2. **Verify LanguageSwitcher** sets localStorage correctly
3. **Check browser** language detection logic

### Performance Concerns?

1. **Lazy load** large translation files if needed
2. **Split translations** by route/feature
3. **Use computeds** for complex translations

## 📁 File Structure

```
arkime/
├── INTERNATIONALIZATION.md              # This guide
├── CONTRIBUTING.md                      # Includes i18n contribution guidelines
├── common/vueapp/
│   ├── locales/                         # Shared translation files
│   │   ├── en.json                     # English (default)
│   │   ├── es.json                     # Spanish
│   │   ├── fr.json                     # French
│   │   ├── de.json                     # German
│   │   ├── ja.json                     # Japanese
│   │   ├── ko.json                     # Korean
│   │   └── zh.json                     # Chinese
│   ├── LanguageSwitcher.vue           # Shared language selector component
│   └── I18nExample.vue                # Shared usage examples
└── viewer/vueapp/src/
    └── main.js                         # Vue I18n configuration
```

## 🔄 Migration Guide

To add i18n to existing components:

1. **Replace hardcoded text** with `$t()` calls
2. **Add translation keys** to all locale files
3. **Test with different languages**
4. **Add LanguageSwitcher** where appropriate

### Example Migration:

**Before:**
```vue
<button>Save Changes</button>
```

**After:**
```vue
<button>{{ $t('common.save') }}</button>
```

And add to locale files:
```json
{
  "common": {
    "save": "Save"
  }
}
```

## 📞 Getting Help

- Check the [Vue I18n documentation](https://vue-i18n.intlify.dev/)
- Look at `I18nExample.vue` for practical examples
- Test your translations with the LanguageSwitcher component

Happy internationalizing! 🌍
