import { formatValue } from '@/utils/formatValue';
import { mapGetters } from 'vuex';
import { applyTemplate } from '@/utils/applyTemplate';
import { applyPostProcess } from '@/utils/applyPostProcess';

export const ITypeMixin = {
  computed: {
    ...mapGetters(['getIntegrations']),
    tidbits () {
      const validTidbits = Object.entries(this.integrationData)
        .flatMap(([integration, data]) => (
          this.getIntegrations[integration].tidbits.map(tidbit => {
            const value = formatValue(data, tidbit);
            const displayValue = this.applyTidbitPostProcess(value, data, integration, tidbit.postProcess, tidbit.template);
            const tooltip = this.applyTidbitTooltip(value, data, displayValue, tidbit.tooltip, tidbit.tooltipTemplate);

            return {
              integration,
              value,
              displayValue,
              tooltip,
              display: tidbit.display,
              label: tidbit.label,
              purpose: tidbit.purpose,
              precedence: tidbit.precedence,
              order: tidbit.order,
              definitionOrder: tidbit.definitionOrder
            };
          })
        ))
        .filter(tidbit => tidbit.value != null && tidbit.value?.length !== 0 &&
                          tidbit.displayValue != null && tidbit.displayValue?.length !== 0); // remove tidbits that failed to get proper data

      // sort by purpose, then precedence -- to allow for purpose-based culling
      validTidbits.sort((a, b) => {
        const purposeDiff = a.purpose.localeCompare(b.purpose);
        if (purposeDiff === 0) {
          return b.precedence - a.precedence; // highest precedence preferred
        }
        return purposeDiff;
      });

      // remove unused fallback tidbits that serve the same purpose as an existing one.
      //   (e.g. use preferred whois datasource)
      const uniqueTidbits = validTidbits.filter((current, i, arr) => {
        if (i === 0 || current.purpose === '') { return true; }
        const previous = arr[i - 1];
        return current.purpose !== previous.purpose || current.precedence == null || previous.precedence == null;
      });

      // sort into final order (by order property first, then integration [alphabetically], then definition order)
      uniqueTidbits.sort((a, b) => {
        const orderDiff = a.order - b.order;
        // when the order is the same, break the tie with integration (alphabetical), then definition order
        if (orderDiff === 0) {
          const integrationDiff = a.integration.localeCompare(b.integration);
          // among the same integration, sort by the order in which they're defined
          return integrationDiff === 0 ? a.definitionOrder - b.definitionOrder : integrationDiff;
        }
        return orderDiff;
      });

      // delete temporary fields
      for (const tidbit of uniqueTidbits) {
        delete tidbit.order;
        delete tidbit.definitionOrder;
        delete tidbit.purpose;
        delete tidbit.precedence;
      }

      return uniqueTidbits;
    },
    integrationData () {
      return this.gatherIntegrationData(this.data, this.itype, this.query);
    }
  },
  methods: {
    applyTidbitTooltip (value, data, displayValue, tooltip, tooltipTemplate) {
      // if the template exists, fill it, otherwise, return the string|undefined tooltip
      return tooltipTemplate
        ? applyTemplate(tooltipTemplate, { value, data, displayValue })
        : tooltip;
    },
    applyTidbitPostProcess (value, data, integration, postProcess, template) {
      if (!value) { return; }
      // first fill template, if existent
      if (template?.length) {
        value = applyTemplate(template, { value, data });
      }
      const uiSettings = this.getIntegrations[integration].uiSettings;
      // apply any post processors
      return applyPostProcess(postProcess, value, { data, uiSettings });
    },
    gatherIntegrationData (data, itype, query) { // restructures data into the shape {[integrationName]: data}
      const iTypeStub = data?.[itype] || {};
      const integrationPairs = Object.entries(iTypeStub)
        .filter(([key, _]) => key !== '_query')
        .map(([integrationName, dataEntryArray]) => [integrationName, dataEntryArray?.find(dataEntry => dataEntry._query === query)?.data])
        .filter(([_, val]) => val != null);

      return Object.fromEntries(integrationPairs);
    }
  }
};
