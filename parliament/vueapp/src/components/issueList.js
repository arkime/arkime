/*
 * Copyright Andy Wick
 * SPDX-License-Identifier: Apache-2.0
 */
const NUMBER_SORTS = new Set(['ignoreUntil', 'firstNoticed', 'lastNoticed', 'acknowledged']);

function collator (locale) {
  try {
    return new Intl.Collator(locale);
  } catch { // a locale the browser doesn't know, like x-pl
    return new Intl.Collator();
  }
}

// The issues that match the search term, sorted. texts(issue) is its translated { title, message },
// the English strings match too since they are what alerts and the API show.
export function issueList (issues, { term, sort, order, locale, texts }) {
  let list = issues;

  if (term) {
    term = term.toLowerCase();
    list = list.filter(issue => [issue.severity, issue.node, issue.cluster, issue.title, issue.message, ...Object.values(texts(issue) ?? {})]
      .some(str => typeof str === 'string' && str.toLowerCase().includes(term)));
  }

  if (sort) {
    const dir = order === 'asc' ? 1 : -1;
    const value = (issue) => sort === 'title' ? texts(issue)?.title : issue[sort];
    const { compare } = collator(locale);
    list = [...list].sort((a, b) => NUMBER_SORTS.has(sort)
      ? dir * ((value(a) ?? 0) - (value(b) ?? 0))
      : dir * compare(String(value(a) ?? ''), String(value(b) ?? '')));
  }

  return list;
}
