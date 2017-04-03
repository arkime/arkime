'use strict';


require('./session/tests/session.service.test');
require('./session/tests/session.list.component.test');
require('./session/tests/session.detail.component.test');
require('./session/tests/session.info.component.test');
require('./session/tests/session.field.component.test');
require('./session/tests/session.actions.component.test');
require('./session/tests/session.delete.component.test');
require('./session/tests/session.export.pcap.component.test');
require('./session/tests/session.export.csv.component.test');
require('./session/tests/session.scrub.pcap.component.test');
require('./session/tests/session.send.component.test');
require('./session/tests/session.tag.component.test');
require('./session/tests/session.sticky.component.test');

require('./search/tests/search.component.test');
require('./search/tests/field.service.test');
require('./search/tests/expression.typeahead.test');

require('./health/tests/eshealth.component.test');
require('./health/tests/health.service.test');

require('./config/config.service.test');

require('./user/tests/user.service.test');
require('./user/tests/has.permission.test');

require('./help/help.test');

// require('./settings/settings.test');

require('./visualization/tests/graph.test');
require('./visualization/tests/map.test');
