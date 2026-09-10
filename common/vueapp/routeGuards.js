/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/

/**
 * Builds a Vue Router `beforeEnter` guard that checks a role (or an
 * arbitrary predicate over the fetched user) before allowing navigation,
 * redirecting to `fallbackRouteName` otherwise.
 * @param {Function} getUser - fetches (and caches in the store) the current user
 * @param {Function} getCachedUser - returns the currently cached user, if any
 * @param {string} fallbackRouteName - route to redirect to when the check fails
 * @param {boolean} [alwaysRefetch] - re-fetch even when a cached user is
 *   already present; use for admin-sensitive routes where a stale cache
 *   shouldn't keep granting access after a role is revoked mid-session
 */
export function createRequireRole (getUser, getCachedUser, fallbackRouteName, { alwaysRefetch = false } = {}) {
  return async function requireRole (check) {
    const passes = typeof check === 'function' ? check : (u) => !!u?.roles?.includes(check);

    let user = alwaysRefetch ? undefined : getCachedUser();
    if (!user) {
      try {
        user = await getUser();
      } catch (err) {
        console.log('ERROR - failed to fetch user for route guard', err);
      }
    }
    if (!passes(user)) { return { name: fallbackRouteName }; }
  };
}
