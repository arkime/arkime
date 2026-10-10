/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { reactive } from 'vue';
import { fetchWrapper } from './fetchWrapper.js';

const DEFAULT_BANNER = { enabled: false, message: '', type: 'info', effects: [], expires: 0, updated: 0 };

// shared reactive state so AppBanner (top of app) updates the moment an
// admin saves/syncs in settings -- no per-app Vuex wiring needed. Each app
// serves /api/banner under its own base href, so a relative url works for all.
const state = reactive({ banner: { ...DEFAULT_BANNER } });

export function bannerState () { return state; }

export async function loadBanner () {
  const banner = await fetchWrapper({ url: 'api/banner' });
  state.banner = banner || { ...DEFAULT_BANNER };
  return state.banner;
}

export async function updateBanner (banner) {
  const res = await fetchWrapper({ url: 'api/banner', method: 'PUT', data: banner });
  if (res.banner) { state.banner = res.banner; }
  return res;
}

export async function syncBanner () {
  const res = await fetchWrapper({ url: 'api/banner/sync', method: 'POST' });
  if (res.banner) { state.banner = res.banner; }
  return res;
}
