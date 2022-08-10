'use-strict';

import { applyPostProcess } from '../src/utils/applyPostProcess';

test('applyPostProcess', () => {
  const uiSettings = {
    testSetting: 'test1,   test2, test3'
  };

  const data = {
    peopleTestValue: [
      { name: 'jimmy', age: 22 },
      { name: 'joe', age: 19 },
      { name: 'bob', age: 23 }
    ],
    nested: {
      personAgeMinimumTestValue: 20
    },
    settingsTestValue: ['test1', 'test2', 'test3']
  };

  const ifAndSettingsPostProcess = [
    {
      if: {
        jsonEquals: {
          setting: 'testSetting',
          postProcess: [{ split: ',' }, { map: ['trim'] }]
        }
      },
      then: 'passed :)',
      else: 'failed >:O'
    }
  ];
  expect(applyPostProcess(ifAndSettingsPostProcess, data.settingsTestValue, data, uiSettings)).toBe('passed :)');

  const mapFilterTemplateAndDataPostProcess = [
    { filter: { value: { with: { pathInto: 'age' } }, greaterThan: { data: 'nested.personAgeMinimumTestValue' } } },
    {
      map: { template: '<value.name> - <value.age>' }
    }
  ];
  expect(applyPostProcess(mapFilterTemplateAndDataPostProcess, data.peopleTestValue, data, uiSettings))
    .toEqual(['jimmy - 22', 'bob - 23']);
});
