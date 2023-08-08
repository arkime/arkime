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

export function localIndicatorId (indicator) {
  return `${indicator.query}-${indicator.itype}`;
}

/**
 * Extracts the indicator object from its global ID
 *   (where a global ID is some number of local IDs separated by commas)
 *
 * @param globalId - the fully qualified indicator path-id
 *           e.g. 'foo.com-domain,1.1.1.1-ip'
 * @returns {Cont3xtIndicator}
 */
export function indicatorFromId (globalId) {
  const localId = globalId.substring(globalId.lastIndexOf(',') + 1);
  const splitAt = localId.lastIndexOf('-');
  return {
    query: localId.substring(0, splitAt),
    itype: localId.substring(splitAt + 1)
  };
}

export function shouldDisplayIntegrationBtn (integration, integrationData) {
  return integrationData != null && integration?.icon != null;
}

export function shouldDisplayCountedIntegrationBtn (integration, integrationData) {
  return shouldDisplayIntegrationBtn(integration, integrationData) && integrationData?._cont3xt?.count != null;
}

export function integrationCountSeverity (integrationData) {
  if (integrationData._cont3xt.count === 0) {
    return 'secondary';
  } else if (integrationData._cont3xt.severity === 'high') {
    return 'danger';
  } else {
    return 'success';
  }
}
