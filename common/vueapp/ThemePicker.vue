<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<!--
  Cross-app theme picker. Used by viewer's Settings.vue and (after
  their bootstrap rip) cont3xt / parliament / wise.

  Renders a grid of theme cards plus a Custom card at the end. Each
  preview is built inline from the theme's own color values -- no
  per-theme CSS. The Custom card expands into an editor with
  v-color-picker swatches, a Load-from-theme menu for forking an
  existing theme, and a copy/paste share string.

  Persistence is parent-driven via two emits:
    - update:modelValue   theme id (string)
    - update:customTheme  full { dark, colors } object
-->
<template>
  <div class="theme-picker">
    <v-row>
      <!-- baked-in themes -->
      <v-col
        v-for="theme in themes"
        :key="theme.id"
        cols="12"
        md="6">
        <v-card
          :class="['theme-card', { 'theme-card--selected': modelValue === theme.id }]"
          variant="outlined"
          @click="$emit('update:modelValue', theme.id)">
          <v-card-title class="d-flex align-center px-2 pt-2 pb-1 theme-card__header">
            <v-icon
              :icon="modelValue === theme.id ? '$radioOn' : '$radioOff'"
              :color="modelValue === theme.id ? 'primary' : undefined"
              class="me-2" />
            <span class="theme-card__name">{{ theme.name }}</span>
            <v-chip
              v-if="theme.dark"
              size="x-small"
              class="ms-2">
              dark
            </v-chip>
          </v-card-title>
          <div
            class="theme-card__preview"
            :style="previewSurfaceStyle(theme)">
            <div
              class="theme-card__navbar"
              :style="navbarStyle(theme)">
              <span
                class="theme-card__navbar-title"
                :style="{ color: theme.colors['on-primary'] || '#fff' }">
                Arkime
              </span>
              <span class="theme-card__navbar-spacer" />
              <span
                class="theme-card__dot"
                :style="{ background: theme.colors.success }"
                title="success" />
              <span
                class="theme-card__dot"
                :style="{ background: theme.colors.info }"
                title="info" />
              <span
                class="theme-card__dot"
                :style="{ background: theme.colors.warning }"
                title="warning" />
              <span
                class="theme-card__dot"
                :style="{ background: theme.colors.error }"
                title="error" />
            </div>
            <div
              class="theme-card__subnavbar"
              :style="subnavbarStyle(theme)">
              <v-icon
                icon="mdi-cog"
                size="x-small"
                class="me-1" />
              <span class="theme-card__subnavbar-title">Settings</span>
            </div>
            <div
              class="theme-card__body"
              :style="{
                background: theme.colors.background,
                color: theme.colors.foreground
              }">
              <div class="theme-card__row">
                Sample text
                <span
                  class="theme-card__accent"
                  :style="{ color: theme.colors['foreground-accent'] }">
                  accent
                </span>
              </div>
              <div class="theme-card__btn-row">
                <span
                  class="theme-card__btn"
                  :style="{
                    background: theme.colors.primary,
                    color: theme.colors['on-primary']
                  }">Primary</span>
                <span
                  class="theme-card__btn"
                  :style="{
                    background: theme.colors.secondary,
                    color: theme.colors['on-secondary']
                  }">Secondary</span>
              </div>
            </div>
          </div>
        </v-card>
      </v-col>

      <!-- custom theme card -->
      <v-col
        cols="12"
        md="6">
        <v-card
          :class="['theme-card', { 'theme-card--selected': modelValue === 'custom1' }]"
          variant="outlined">
          <v-card-title
            class="d-flex align-center px-2 pt-2 pb-1 theme-card__title-bar theme-card__header"
            @click="onCustomCardClick">
            <v-icon
              :icon="modelValue === 'custom1' ? '$radioOn' : '$radioOff'"
              :color="modelValue === 'custom1' ? 'primary' : undefined"
              class="me-2" />
            <span class="theme-card__name">Custom</span>
            <v-spacer />
            <v-btn
              v-if="customTheme"
              size="large"
              variant="text"
              @click.stop="editorOpen = !editorOpen">
              <v-icon
                :icon="editorOpen ? '$collapse' : '$expand'"
                class="me-1" />
              {{ editorOpen ? 'Hide colors' : 'Edit colors' }}
            </v-btn>
          </v-card-title>

          <!-- empty state: offer initialization -->
          <div
            v-if="!customTheme"
            class="theme-card__custom-empty">
            <p class="mb-3 text-medium-emphasis">
              Build your own theme. We'll start with a copy of
              <strong>{{ activeThemeName }}</strong> so you have a
              working baseline to tweak.
            </p>
            <v-btn
              size="large"
              color="primary"
              variant="flat"
              @click="initCustomFromActive">
              <v-icon
                start
                icon="$plus" />
              Create custom theme
            </v-btn>
          </div>

          <!-- mini-preview rendered from the custom theme's colors -->
          <div
            v-else
            class="theme-card__preview"
            :style="previewSurfaceStyle(customTheme)"
            @click.stop="$emit('update:modelValue', 'custom1')">
            <div
              class="theme-card__navbar"
              :style="navbarStyle(customTheme)">
              <span
                class="theme-card__navbar-title"
                :style="{ color: customTheme.colors['on-primary'] || '#fff' }">
                Arkime (custom)
              </span>
              <span class="theme-card__navbar-spacer" />
              <span
                class="theme-card__dot"
                :style="{ background: customTheme.colors.success }" />
              <span
                class="theme-card__dot"
                :style="{ background: customTheme.colors.info }" />
              <span
                class="theme-card__dot"
                :style="{ background: customTheme.colors.warning }" />
              <span
                class="theme-card__dot"
                :style="{ background: customTheme.colors.error }" />
            </div>
            <div
              class="theme-card__subnavbar"
              :style="subnavbarStyle(customTheme)">
              <v-icon
                icon="mdi-cog"
                size="x-small"
                class="me-1" />
              <span class="theme-card__subnavbar-title">Settings</span>
            </div>
            <div
              class="theme-card__body"
              :style="{
                background: customTheme.colors.background,
                color: customTheme.colors.foreground
              }">
              <div class="theme-card__row">
                Sample text
                <span
                  class="theme-card__accent"
                  :style="{ color: customTheme.colors['foreground-accent'] }">
                  accent
                </span>
              </div>
              <div class="theme-card__btn-row">
                <span
                  class="theme-card__btn"
                  :style="{
                    background: customTheme.colors.primary,
                    color: customTheme.colors['on-primary']
                  }">Primary</span>
                <span
                  class="theme-card__btn"
                  :style="{
                    background: customTheme.colors.secondary,
                    color: customTheme.colors['on-secondary']
                  }">Secondary</span>
              </div>
            </div>
          </div>

          <!-- color editor -->
          <div
            v-if="customTheme && editorOpen"
            class="theme-card__editor pa-3">
            <!-- fork-from-existing-theme menu: overwrites custom1's
                 colors with a clone of a baked-in theme so the user
                 can use any theme as a starting point -->
            <v-menu>
              <template #activator="{ props: activatorProps }">
                <v-btn
                  v-bind="activatorProps"
                  size="large"
                  color="primary"
                  variant="tonal"
                  class="mb-3">
                  Load colors from theme
                  <v-icon
                    end
                    icon="$expand" />
                </v-btn>
              </template>
              <v-list density="compact">
                <v-list-item
                  v-for="theme in themes"
                  :key="theme.id"
                  @click="loadFromTheme(theme)">
                  <template #prepend>
                    <span
                      class="theme-card__menu-swatch"
                      :style="{ background: theme.colors.primary }" />
                  </template>
                  <v-list-item-title>{{ theme.name }}</v-list-item-title>
                </v-list-item>
              </v-list>
            </v-menu>

            <p class="text-caption text-medium-emphasis mb-2">
              Click a swatch to edit. Shade variants (lighter/darker)
              are computed automatically.
            </p>
            <v-row
              v-for="(group, gi) in EDITABLE_GROUPS"
              :key="gi"
              dense>
              <v-col
                v-for="key in group"
                :key="key"
                cols="6"
                sm="4">
                <div class="theme-card__editor-row">
                  <v-menu
                    :close-on-content-click="false"
                    location="bottom start"
                    @update:model-value="onSwatchMenuToggle(key, $event)">
                    <template #activator="{ props: activatorProps }">
                      <button
                        type="button"
                        v-bind="activatorProps"
                        class="theme-card__editor-swatch"
                        :style="{ background: customTheme.colors[key] || '#000' }"
                        :title="`Edit ${key}`" />
                    </template>
                    <template #default="{ isActive }">
                      <v-card class="theme-card__color-popover pa-2">
                        <v-color-picker
                          :model-value="getPendingHex(key)"
                          mode="hex"
                          hide-mode-switch
                          hide-inputs
                          flat
                          @update:model-value="setPendingHex(key, $event)" />
                        <div class="d-flex justify-end mt-2">
                          <v-btn
                            size="large"
                            color="primary"
                            variant="flat"
                            @click="commitPendingHex(key); isActive.value = false">
                            <v-icon
                              start
                              icon="$complete" />
                            Apply
                          </v-btn>
                        </div>
                      </v-card>
                    </template>
                  </v-menu>
                  <span class="theme-card__editor-key">{{ key }}</span>
                </div>
              </v-col>
            </v-row>

            <!-- shareable code: copy this to share, paste a string to apply -->
            <v-divider class="my-3" />
            <p class="text-caption text-medium-emphasis mb-1">
              Share this theme: copy the code below, or paste someone
              else's code and press Enter (or click Apply) to load it.
            </p>
            <div class="theme-card__share-row">
              <v-text-field
                v-model="shareInput"
                density="compact"
                hide-details
                variant="outlined"
                spellcheck="false"
                placeholder="background:#fff,primary:#214b78,..."
                class="theme-card__share-field"
                @keydown.enter.prevent="applyShareInput"
                @blur="applyShareInput" />
              <v-btn
                size="large"
                color="primary"
                variant="tonal"
                class="ms-2"
                @click="copyShareCode">
                <v-icon
                  v-if="copyStatus === 'ok'"
                  start
                  icon="$complete" />
                {{ copyStatus === 'ok' ? 'Copied' : 'Copy' }}
              </v-btn>
              <v-btn
                size="large"
                color="primary"
                variant="tonal"
                class="ms-2"
                @click="applyShareInput">
                Apply
              </v-btn>
            </div>
          </div>
        </v-card>
      </v-col>
    </v-row>
  </div>
</template>

<script>
import { expandShades, isDark } from './themes/colorShades.js';
import {
  encodeShareableTheme,
  applyShareableTheme
} from './themes/customTheme.js';

export default {
  name: 'ThemePicker',
  emits: ['update:modelValue', 'update:customTheme'],
  props: {
    modelValue: { type: String, required: true },
    themes: { type: Array, required: true },
    customTheme: {
      type: Object,
      default: null
    }
  },
  data () {
    return {
      // Visual grouping in the editor: the map (water/land) and
      // traffic (src/dst) pairs each get their own row so they read
      // as a related set instead of getting mixed in with the
      // semantic colors.
      EDITABLE_GROUPS: [
        ['background', 'foreground', 'foreground-accent',
          'primary', 'secondary', 'success', 'info',
          'warning', 'error'],
        ['water', 'land'],
        ['src', 'dst']
      ],
      // Default to open so a returning user with an existing custom
      // theme sees the editable swatches without an extra click; user
      // can collapse via the Hide colors button.
      editorOpen: true,
      shareInput: '',
      copyStatus: 'idle',
      // Per-swatch staging: v-color-picker emits on every drag tick;
      // we hold the pending value locally and only commit (-> emit
      // update:customTheme -> server save) when the user clicks
      // Apply or closes the popover.
      pendingHex: {}
    };
  },
  computed: {
    activeThemeName () {
      const match = this.themes.find(theme => theme.id === this.modelValue);
      return match ? match.name : 'Arkime Light';
    }
  },
  watch: {
    customTheme: {
      handler (val) {
        const canonical = encodeShareableTheme(val);
        if (this.shareInput !== canonical) this.shareInput = canonical;
      },
      immediate: true,
      deep: true
    }
  },
  methods: {
    previewSurfaceStyle (theme) {
      return { background: theme.colors.background };
    },
    navbarStyle (theme) {
      return {
        background: theme.colors.primary,
        color: theme.colors['on-primary'] || '#fff'
      };
    },
    subnavbarStyle (theme) {
      // Match the real sub-navbar (.sub-navbar): secondary-lightest
      // background, foreground text (it sets no explicit text color).
      return {
        background: theme.colors['secondary-lightest'],
        color: theme.colors.foreground
      };
    },
    onCustomCardClick () {
      if (!this.customTheme) {
        this.initCustomFromActive();
      } else {
        this.$emit('update:modelValue', 'custom1');
      }
    },
    /* Overwrite the custom theme's colors with a deep clone of a
       baked-in theme. Used when the user wants to fork an existing
       theme as a starting point for their custom one. */
    loadFromTheme (theme) {
      if (!theme || !theme.colors) return;
      const cloned = { dark: !!theme.dark, colors: { ...theme.colors } };
      this.pendingHex = {};
      this.$emit('update:customTheme', cloned);
      if (this.modelValue !== 'custom1') {
        this.$emit('update:modelValue', 'custom1');
      }
    },
    initCustomFromActive () {
      // Clone from the currently-selected manifest theme. Custom1
      // itself is never in `this.themes` (it's runtime-registered),
      // so when the active selection is 'custom1' but no customTheme
      // prop exists (e.g. a stale settings row from before the API
      // allowlist included customTheme), fall back to a baked-in
      // theme so we don't no-op or clone a phantom.
      const source = this.themes.find(t => t.id === this.modelValue && t.id !== 'custom1')
        || this.themes.find(t => t.id === 'arkime-light')
        || this.themes[0];
      const cloned = { dark: source.dark, colors: { ...source.colors } };
      this.$emit('update:customTheme', cloned);
      this.$emit('update:modelValue', 'custom1');
      this.editorOpen = true;
    },
    /* Coerce arbitrary input into #RRGGBB. expandShades NaNs out on
       non-hex input, so we sanitize before forwarding. */
    normalizeHex (raw) {
      if (typeof raw !== 'string') return '#000000';
      const s = raw.trim();
      if (/^#[0-9a-fA-F]{6}$/.test(s)) return s;
      if (/^#[0-9a-fA-F]{3}$/.test(s)) {
        return '#' + s.slice(1).split('').map(c => c + c).join('');
      }
      return '#000000';
    },
    updateColor (key, value) {
      const hex = this.normalizeHex(value);
      const baseColors = this.customTheme && this.customTheme.colors
        ? this.customTheme.colors
        : {};
      const newColors = { ...baseColors, [key]: hex };

      if (['primary', 'secondary'].includes(key)) {
        Object.assign(newColors, expandShades(key, hex));
      }
      // tertiary aliases success; quaternary aliases info -- keep both
      // sets of shade keys populated for any legacy consumers.
      if (key === 'success') {
        Object.assign(newColors, expandShades('success', hex));
        Object.assign(newColors, expandShades('tertiary', hex));
      }
      if (key === 'info') {
        Object.assign(newColors, expandShades('info', hex));
        Object.assign(newColors, expandShades('quaternary', hex));
      }

      const dark = key === 'background'
        ? isDark(hex)
        : (this.customTheme ? this.customTheme.dark : false);

      this.$emit('update:customTheme', { dark, colors: newColors });
    },
    /* Per-swatch staged-color helpers. v-color-picker emits on every
       slider drag tick; we stage in pendingHex and only commit on
       Apply or popover close to avoid spamming the server save. */
    getPendingHex (key) {
      return this.pendingHex[key] != null
        ? this.pendingHex[key]
        : this.normalizeHex(this.customTheme && this.customTheme.colors[key]);
    },
    setPendingHex (key, hex) {
      if (typeof hex === 'string' && hex) {
        this.pendingHex[key] = hex;
      }
    },
    commitPendingHex (key) {
      const hex = this.pendingHex[key];
      if (hex) this.updateColor(key, hex);
      delete this.pendingHex[key];
    },
    onSwatchMenuToggle (key, isOpen) {
      // Closing via click-outside should still commit the pending
      // change -- matches the prior native-input behavior where
      // dismissing the picker applied the selected color.
      if (!isOpen) this.commitPendingHex(key);
    },
    copyShareCode () {
      const code = encodeShareableTheme(this.customTheme);
      if (!code) return;
      const done = () => {
        this.copyStatus = 'ok';
        setTimeout(() => { this.copyStatus = 'idle'; }, 1500);
      };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(code).then(done).catch(() => {});
      } else {
        // Fallback for non-clipboard contexts (some embedded views).
        const ta = document.createElement('textarea');
        ta.value = code;
        document.body.appendChild(ta);
        ta.select();
        try { document.execCommand('copy'); done(); } catch (e) { /* noop */ }
        document.body.removeChild(ta);
      }
    },
    applyShareInput () {
      const canonical = encodeShareableTheme(this.customTheme);
      // Nothing changed -- bail (also avoids spurious emits on blur).
      if (this.shareInput.trim() === canonical) return;
      const next = applyShareableTheme(this.customTheme, this.shareInput);
      if (next) {
        this.$emit('update:customTheme', next);
      } else {
        // Invalid input -- snap back to canonical so the user sees
        // that nothing was applied.
        this.shareInput = canonical;
      }
    }
  }
};
</script>

<style scoped>
.theme-picker {
  width: 100%;
}
.theme-card {
  cursor: pointer;
  transition: border-color 0.15s ease, box-shadow 0.15s ease;
}
.theme-card:hover {
  border-color: rgb(var(--v-theme-primary));
}
.theme-card--selected {
  border-color: rgb(var(--v-theme-primary)) !important;
  border-width: 2px !important;
  box-shadow: 0 0 0 1px rgb(var(--v-theme-primary));
}
.theme-card__name {
  font-weight: 600;
}
/* Whole Custom title-bar is clickable to select the theme; cursor
   hint so users know the gap between "Custom" and the Edit button
   isn't dead space. */
.theme-card__title-bar {
  cursor: pointer;
}
/* Faint tinted band so the radio/name control reads as a distinct header,
   clearly separated from the theme preview below (two different surfaces
   meeting at the divider line make the boundary unambiguous). */
.theme-card__header {
  background: rgba(var(--v-theme-foreground), 0.16);
}
.theme-card__preview {
  display: flex;
  flex-direction: column;
  /* Faded foreground reads as a visible hairline in both light and dark
     modes (Vuetify's default divider opacity is too faint on light cards). */
  border-top: 1px solid rgba(var(--v-theme-foreground), 0.28);
}
.theme-card__navbar {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 8px 10px;
  font-size: 0.85rem;
}
.theme-card__navbar-title {
  font-weight: 600;
}
.theme-card__navbar-spacer {
  flex: 1;
}
.theme-card__dot {
  display: inline-block;
  width: 14px;
  height: 14px;
  border-radius: 50%;
  border: 1px solid rgba(0, 0, 0, 0.15);
}
.theme-card__subnavbar {
  display: flex;
  align-items: center;
  padding: 5px 10px;
  font-size: 0.78rem;
}
.theme-card__subnavbar-title {
  font-weight: 700;
}
.theme-card__body {
  padding: 12px;
  font-size: 0.85rem;
}
.theme-card__row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 8px;
}
.theme-card__accent {
  font-weight: 600;
}
.theme-card__btn-row {
  display: flex;
  gap: 6px;
}
.theme-card__btn {
  display: inline-flex;
  align-items: center;
  padding: 4px 10px;
  font-size: 0.75rem;
  font-weight: 600;
  border-radius: 4px;
}

.theme-card__custom-empty {
  padding: 24px;
  text-align: center;
  border-top: 1px solid rgba(var(--v-theme-foreground), 0.28);
}
.theme-card__editor {
  border-top: 1px solid rgba(0, 0, 0, 0.1);
}
.theme-card__editor-row {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 4px 0;
}
.theme-card__editor-swatch {
  display: inline-block;
  width: 22px;
  height: 22px;
  padding: 0;
  border-radius: 4px;
  border: 1px solid rgba(0, 0, 0, 0.2);
  cursor: pointer;
  flex-shrink: 0;
}
.theme-card__color-popover {
  width: max-content;
}
.theme-card__menu-swatch {
  display: inline-block;
  width: 16px;
  height: 16px;
  border-radius: 3px;
  border: 1px solid rgba(0, 0, 0, 0.2);
  margin-right: 8px;
  flex-shrink: 0;
}
.theme-card__editor-key {
  font-size: 0.78rem;
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.theme-card__share-row {
  display: flex;
  align-items: center;
}
.theme-card__share-field :deep(input) {
  font-family: SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 0.78rem;
}
</style>
