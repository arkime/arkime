const Cont3xtUtil = {
  isString (maybeStr, minLength = 1) {
    return typeof maybeStr === 'string' && maybeStr.length >= minLength;
  }
};

/**
 * @typedef {{ query: string, itype: 'domain' | 'ip' | 'url' | 'email' | 'phone' | 'hash' | 'text' }} Cont3xtIndicator
 */

/**
 * @typedef {{ indicator: Cont3xtIndicator, children: Cont3xtIndicatorNode[], parentIds: Set<string|undefined>, enhanceInfo: object }} Cont3xtIndicatorNode
 */

export const Cont3xtIndicatorProp = {
  validator (maybeIndicator) {
    return (maybeIndicator != null && typeof maybeIndicator === 'object') &&
      (Cont3xtUtil.isString(maybeIndicator?.itype) && Cont3xtUtil.isString(maybeIndicator?.query));
  }
};

export function getIntegrationDataMap (results, indicator) {
  return results?.[indicator.itype]?.[indicator.query] ?? {};
}

export function getIntegrationData (results, indicator, source) {
  return results?.[indicator.itype]?.[indicator.query]?.[source] ?? {};
}

export function indicatorId (indicator) {
  return `${indicator.query}-${indicator.itype}`;
}
