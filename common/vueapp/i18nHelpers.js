/**
 * Copyright Yahoo Inc.
 * SPDX-License-Identifier: Apache-2.0
 **/

// For use with option menu items, e.g. v-i18n-value="'settings.something-.'" = settings.something-[value]
export const i18nValue = {
  mounted(el, binding) {
    const vm = binding.instance;
    if (vm && vm.$t) {
      el.textContent = vm.$t(binding.value + el.value)
    }
  },
  updated(el, binding) {
    const vm = binding.instance;
    if (vm && vm.$t) {
      el.textContent = vm.$t(binding.value + el.value)
    }
  }
};

// For use with BTooltip items, e.g. v-i18n-btip="'settings.options.'" = settings.options.[value][targetId]Tip
export const i18nBTip = {
  mounted(el, binding) {
    const vm = binding.instance;
    let target = el.parentElement?.parentElement?.parentElement?.parentElement;
    if (vm && vm.$t && target !== undefined) {
      el.textContent = vm.$t(binding.value + target.id + 'Tip');
    }
  },
  updated(el, binding) {
    const vm = binding.instance;
    let target = el.parentElement?.parentElement?.parentElement?.parentElement;
    if (vm && vm.$t && target !== undefined) {
      el.textContent = vm.$t(binding.value + target.id + 'Tip');
    }
  }
};

// For use with b-dropdown-item items, e.g. v-i18n-bdd="'settings.options.'" = settings.options.[id]
export const i18nBDD = {
  mounted(el, binding, vnode) {
    const vm = binding.instance;
    let target = el.children[0];
    if (vm && vm.$t && target !== undefined) {
      target.textContent = vm.$t(binding.value + target.id);
    }
  },
  updated(el, binding) {
    const vm = binding.instance;
    let target = el.children[0];
    if (vm && vm.$t && target !== undefined) {
      target.textContent = vm.$t(binding.value + target.id);
    }
  }
};
