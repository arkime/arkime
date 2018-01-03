'use strict';


require('./user/user.service');
require('./user/components/has.permission');
require('./user/components/create.view');
require('./user/components/user.list');

require('./session/services/session.service');
require('./session/components/session.list.component');
require('./session/components/session.detail.component');
require('./session/components/session.tag.component');
require('./session/components/session.actions.component');
require('./session/components/session.sticky.component');
require('./session/components/session.export.pcap.component');
require('./session/components/session.export.csv.component');
require('./session/components/session.scrub.pcap.component');
require('./session/components/session.delete.component');
require('./session/components/session.send.component');
require('./session/components/session.info.component');
require('./session/components/session.field.component');
require('./session/components/session.field.menu.component');

require('./search/components/search.component');
require('./search/components/expression.typeahead');
require('./search/components/time.component');
require('./search/services/field.service');

require('./health/health.service');
require('./health/eshealth.component');

require('./config/config.service');

require('./help/help');

require('./settings/settings');

require('./files/files.service');
require('./files/files');

require('./stats/stats.service');
require('./stats/stats.component');
require('./stats/stats.es.component');
require('./stats/stats.indices.component');
require('./stats/stats.tasks.component');
require('./stats/stats.shards.component');

require('./spigraph/spigraph.component');
require('./spigraph/spigraph.service');

require('./spiview/spiview');
require('./spiview/spiview.service');

require('./connections/connections.component');
require('./connections/connections.service');
require('./connections/connections.nodepopup.component');
require('./connections/connections.linkpopup.component');

require('./visualization/components/graph.map');
require('./visualization/components/graph');
require('./visualization/components/map');

require('./upload/upload');

require('./history/history');
require('./history/history.service');

require('./404/404');
