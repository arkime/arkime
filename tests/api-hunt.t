use Test::More tests => 314;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $token = getTokenCookie();
my $otherToken = getTokenCookie('user2');
my $nonadminToken = getTokenCookie2('user3');
my $json;

# Delete old hunts
  esPost("/tests_hunts/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');

# Make sure no hunts
  my $hunts = viewerGet("/hunt/list?all");
  eq_or_diff($hunts, from_json('{"recordsTotal": 0, "data": [], "recordsFiltered": 0}'));

# Create huntuser
  $json = viewerPostToken("/user/create", '{"userId": "huntuser", "userName": "UserName", "enabled":true, "password":"password", "packetSearch":true}', $token);

my $hToken = getTokenCookie('huntuser');

##### ERRORS
# Must have token to add a hunt
  $json = viewerPost("/hunt", '{"totalSessions":1,"name":"test hunt 1","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}');
  eq_or_diff($json, from_json('{"text": "Missing token", "success": false}'));

# Must apply to sessions to add a hunt
  $json = viewerPostToken("/api/hunt", '{"totalSessions":0,"name":"test hunt 2","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}', $token);
  eq_or_diff($json, from_json('{"text": "This hunt does not apply to any sessions", "success": false}'));

# Must have a name to add a hunt
  $json = viewerPostToken("/hunt", '{"totalSessions":1,"size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}', $token);
  eq_or_diff($json, from_json('{"text": "Missing hunt name", "success": false}'));

# Must have a size to add a hunt
  $json = viewerPostToken("/hunt", '{"totalSessions":1,"name":"test hunt 3","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}', $token);
  eq_or_diff($json, from_json('{"text": "Missing max mumber of packets to examine per session", "success": false}'));

# Must have search text to add a hunt
  $json = viewerPostToken("/hunt", '{"totalSessions":1,"name":"test hunt 4","size":"50","searchType":"ascii","type":"raw","src":true,"dst":true}', $token);
  eq_or_diff($json, from_json('{"text": "Missing packet search text", "success": false}'));

# Must have search text type to add a hunt
  $json = viewerPostToken("/api/hunt", '{"totalSessions":1,"name":"test hunt 5","size":"50","search":"test search text","type":"raw","src":true,"dst":true, "query": {"startTime":0, "stopTime":1}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing packet search text type", "success": false}'));

# Must have a valid search text type to add a hunt
  $json = viewerPostToken("/hunt", '{"totalSessions":1,"name":"test hunt 6","size":"50","search":"test search text","searchType":"asdf","type":"raw","src":true,"dst":true, "query": {"startTime":0, "stopTime":1}}', $token);
  eq_or_diff($json, from_json('{"text": "Improper packet search text type. Must be \"ascii\", \"asciicase\", \"hex\", \"hexregex\", or \"regex\"", "success": false}'));

# Must have a type to add a hunt
  $json = viewerPostToken("/hunt",'{"totalSessions":1,"name":"test hunt 7","size":"50","search":"test search text","searchType":"ascii","src":true,"dst":true, "query": {"startTime":0, "stopTime":1}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing packet search type (raw or reassembled packets)", "success": false}'));

# Must have a valid type to add a hunt
  $json = viewerPostToken("/hunt",'{"totalSessions":1,"name":"test hunt 8","size":"50","search":"test search text","searchType":"ascii","type":"asdf","src":true,"dst":true, "query": {"startTime":0, "stopTime":1}}', $token);
  eq_or_diff($json, from_json('{"text": "Improper packet search type. Must be \"raw\" or \"reassembled\"", "success": false}'));

# Must have src or dst to add a hunt
  $json = viewerPostToken("/api/hunt", '{"totalSessions":1,"name":"test hunt 9","size":"50","search":"test search text","searchType":"ascii","type":"raw"}', $token);
  eq_or_diff($json, from_json('{"text": "The hunt must search source or destination packets (or both)", "success": false}'));

# Must have query to add a hunt
  $json = viewerPostToken("/hunt", '{"totalSessions":1,"name":"test hunt 10","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}', $token);
  eq_or_diff($json, from_json('{"text": "Missing query", "success": false}'));

# Must have fully formed query to add a hunt
  $json = viewerPostToken("/api/hunt", '{"totalSessions":1,"name":"test hunt 11","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing fully formed query (must include start time and stop time)", "success": false}'));

  $json = viewerPostToken("/hunt", '{"totalSessions":1,"name":"test hunt 12","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"stopTime":1536872891}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing fully formed query (must include start time and stop time)", "success": false}'));

# Make sure no hunts
  my $hunts = viewerGet("/api/hunts?all");
  eq_or_diff($hunts, from_json('{"recordsTotal": 0, "data": [], "recordsFiltered": 0}'));

##### GOOD

# Add a valid hunt, and it should immediately run
  $json = viewerPostToken("/hunt?molochRegressionUser=anonymous", '{"totalSessions":1,"name":"test hunt 13~`!@#$%^&*()[]{};<>?/`","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}', $token);
  is ($json->{success}, 1);
  my $id1 = $json->{hunt}->{id};

# cancel hunt
  my $canceljson = viewerPutToken("/api/hunt/$id1/cancel?molochRegressionuser=anonymous", "{}", $token);
  is ($canceljson->{success}, 1, "can cancel a hunt");

# Make sure the hunt's name doesn't contain special chars
  is ($json->{hunt}->{name}, "test hunt 13", "Strip special chars");

# Hunt should finish
  viewerGet("/regressionTests/processHuntJobs");

  $hunts = viewerGet("/api/hunts?molochRegressionUser=user2&history=true");
  is (@{$hunts->{data}}, 1, "Add hunt 1");

# user2 shouldn't see id, query, search, searchType, userId
  my $item = $hunts->{data}->[0];
  is($item->{id}, '');
  is($item->{search}, '');
  is($item->{searchType}, '');
  is($item->{userId}, '');
  ok(! exists $item->{query});

# If the user is not an admin they can only delete their own hunts
  $json = viewerDeleteToken("/api/hunt/$id1?molochRegressionUser=user2", $otherToken);
  is ($json->{text}, "You cannot change another user's hunt unless you have admin privileges");

  $hunts = viewerGet("/hunt/list?history=true");
  is (@{$hunts->{data}}, 1, "Non admin user cannot delete another user's hunt");

  $json = viewerPostToken("/hunt?molochRegressionUser=user2", '{"totalSessions":1,"name":"test hunt 14","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}', $otherToken);

  viewerGet("/regressionTests/processHuntJobs");

  $hunts = viewerGet("/hunt/list?history=true");
  is (@{$hunts->{data}}, 2, "Add hunt 2");

  my $id2 = $json->{hunt}->{id};

  $json = viewerDeleteToken("/hunt/$id2?molochRegressionUser=user2", $otherToken);
  is ($json->{text}, "Deleted hunt successfully");

  $hunts = viewerGet("/hunt/list?history=true");
  is (@{$hunts->{data}}, 1, "User can remove their own hunt");

# If the user is not an admin they can only pause their own hunts
  $json = viewerPostToken("/hunt?molochRegressionUser=anonymous", '{"totalSessions":1,"name":"test hunt 15~`!@#$%^&*()[]{};<>?/`","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}', $token);
  my $id3 = $json->{hunt}->{id};

  $json = viewerPutToken("/api/hunt/$id3/pause?molochRegressionUser=user2", "{}", $otherToken);
  is ($json->{text}, "You cannot change another user\'s hunt unless you have admin privileges", "Non admin user cannot pause another user's hunt");

# If the user is not an admin they can only play their own hunts
  $json = viewerPutToken("/api/hunt/$id3/play?molochRegressionUser=user2", "{}", $otherToken);
  is ($json->{text}, "You cannot change another user\'s hunt unless you have admin privileges", "Non admin user cannot pause another user's hunt");

  $json = viewerDeleteToken("/hunt/$id3?molochRegressionUser=anonymous", $token);
  is ($json->{text}, "Deleted hunt successfully");

# Admin can delete any hunt
  $json = viewerPostToken("/hunt?molochRegressionUser=user2", '{"totalSessions":1,"name":"test hunt 16","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}', $otherToken);
  my $id4 = $json->{hunt}->{id};

  sleep(2); # Wait for it to finish processing

  $json = viewerDeleteToken("/hunt/$id4?molochRegressionUser=anonymous", $token);
  is ($json->{text}, "Deleted hunt successfully");

  $hunts = viewerGet("/hunt/list?history=true");
  my $found = 0;
  foreach my $item (@{$hunts->{data}}) {
    if ($item->{id} eq $id4) {
      $found = 1;
      last;
    }
  }
  is ($found, 0, "Admin can remove any hunt");

# should be able to run a hunt with a view
  $json = viewerPostToken("/api/view?molochRegressionUser=user2", '{"name": "tls", "expression": "protocols == tls", "users": "user2,user3", "roles":["arkimeUser"]}', $otherToken);
  my $viewId = $json->{view}->{id};
  $json = viewerPostToken("/hunt?molochRegressionUser=user2", '{"totalSessions":1,"name":"test hunt 13~`!@#$%^&*()[]{};<>?/`","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891,"view":"' . $viewId . '"}}', $otherToken);
  is ($json->{success}, 1, "can run a hunt with a view");
  my $id5 = $json->{hunt}->{id};

# hunt bad regex
  $json = viewerPostToken("/hunt?molochRegressionUser=user2", '{"totalSessions":1,"name":"badhunt","size":"50","search":"bad[","searchType":"regex","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}', $otherToken);
  my $id6 = $json->{hunt}->{id};

  viewerGet("/regressionTests/processHuntJobs");

  $hunts = viewerGet("/hunt/list?all");
  my ($viewHunt, $badHunt);
  foreach my $item (@{$hunts->{data}}) {
    if ($item->{id} eq $id5) {
      $viewHunt = $item;
    }
    if ($item->{id} eq $id6) {
      $badHunt = $item;
    }
  }

# verify viewHunt and badHunt
  is($viewHunt->{query}->{view}, $viewId, "hunt has a view applied");
  is($viewHunt->{unrunnable}, undef, "hunt should be runable");

  is($badHunt->{unrunnable}, 1, "hunt should be unrunable");

# add a hunt that is shared with another user
  $json = viewerPostToken("/hunt?molochRegressionUser=anonymous", '{"users":"huntuser","totalSessions":1,"name":"test hunt 13~`!@#$%^&*()[]{};<>?/`","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}', $token);
  esGet("/_refresh");
  is ($json->{hunt}->{users}->[0], "huntuser", "hunt should have a user on creation");
  my $id7 = $json->{hunt}->{id};

# remove a user from a hunt
  sleep(1); # Wait for user to be set or else test after next fails
  $json = viewerDeleteToken("/api/hunt/$id7/user/huntuser?molochRegressionUser=anonymous", $token);
  is (scalar @{$json->{users}}, 0, "hunt should have no users");

# can't delete a user from an hunt with no users
  $json = viewerDeleteToken("/api/hunt/$id7/user/huntuser?molochRegressionUser=anonymous", $token);
  eq_or_diff($json, from_json('{"text": "There are no users that have access to view this hunt", "success": false}'), "can't delete a user from an hunt with no users");

# add a user to a hunt
  $json = viewerPostToken("/hunt/$id7/users?molochRegressionUser=anonymous", '{"users":"huntuser"}', $token);
  is ($json->{users}->[0], "huntuser", "hunt should have a user added");

# can't add an unknown user to a hunt
  $json = viewerPostToken("/api/hunt/$id7/users?molochRegressionUser=anonymous", '{"users":"unknownuser"}', $token);
  eq_or_diff($json, from_json('{"text": "Unable to validate user IDs provided", "success": false}'), "hunt should show error if no users are added");

# hunt should not add and send back invalid users
  $json = viewerDeleteToken("/hunt/$id7/users/huntuser?molochRegressionUser=anonymous", $token);
  $json = viewerPostToken("/hunt/$id7/users?molochRegressionUser=anonymous", '{"users":"huntuser,unknownuser"}', $token);
  is (scalar @{$json->{users}}, 1, "hunt should not add an unknown user");
  is ($json->{invalidUsers}->[0], "unknownuser", "hunt should send back invalid users");

# can't add empty users
  $json = viewerPostToken("/api/hunt/$id7/users?molochRegressionUser=anonymous", '{"users":""}', $token);
  eq_or_diff($json, from_json('{"success":false,"text":"You must provide users in a comma separated string"}'), "hunt can't add empty users");

# can't delete an unknown user
  $json = viewerDeleteToken("/api/hunt/$id7/user/unknownuser?molochRegressionUser=anonymous", $token);
  eq_or_diff($json, from_json('{"text": "That user does not have access to this hunt", "success": false}'), "can't delete a user from an hunt with no users");

# remove hunt id and name from sessions
  $json = viewerPutToken("/api/hunt/$id7/removefromsessions?molochRegressionUser=anonymous", "{}", $token);
  is ($json->{success}, 0, "can't remove hunt name and id from hunts with no matches");
  $json = viewerPostToken("/hunt?molochRegressionUser=anonymous", '{"totalSessions":1,"name":"test hunt","size":"50","search":"coconut","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}', $token);
  my $id8 = $json->{hunt}->{id};
  viewerGet("/regressionTests/processHuntJobs");
  $json = viewerPutToken("/api/hunt/$id8/removefromsessions?molochRegressionUser=anonymous", "{}", $token);
  is ($json->{success}, 1, "can remove hunt name and id from sessions");

# can update hunt description
  $json = viewerPutToken("/api/hunt/$id7", '{"description":"awesome new description"}', $token);
  is ($json->{success}, 1, "can update hunt description");
  $hunts = viewerGet("/hunt/list?all");
  diag Dumper($hunts);
  is ($hunts->{data}->[4]->{description}, "awesome new description", "description updated");

# validate that user can't access hunt secret fields because of hunt roles
  $hunts = viewerGetToken("/hunt/list?all&molochRegressionUser=user3", $nonadminToken);
  my ($viewHunt, $badHunt);
  foreach my $item (@{$hunts->{data}}) {
    is ($item->{id}, "", "should be missing id field");
    is ($item->{userId}, "", "should be missing userId field");
    is ($item->{search}, "", "should be missing search field");
    is ($item->{searchType}, "", "should be missing searchType field");
    is ($item->{query}, undef, "should be missing query field");
  }

# can update hunt roles
  $json = viewerPutToken("/api/hunt/$id7", '{"roles":["arkimeUser"]}', $token);
  is ($json->{success}, 1, "can update hunt roles");
  $hunts = viewerGet("/hunt/list?all");
  diag Dumper($hunts);
  is ($hunts->{data}->[4]->{roles}->[0], "arkimeUser", "roles updated");

# validate that user can access hunt secrets now that the role is set
  $hunts = viewerGetToken("/hunt/list?all&molochRegressionUser=user3", $nonadminToken);
  ok(exists $hunts->{data}->[4]->{id});
  isnt($hunts->{data}->[4]->{id}, "", "should have id field");
  ok(exists $hunts->{data}->[4]->{userId});
  isnt($hunts->{data}->[4]->{userId}, "", "should be missing userId field");
  ok(exists $hunts->{data}->[4]->{search});
  isnt($hunts->{data}->[4]->{search}, "", "should be missing search field");
  ok(exists $hunts->{data}->[4]->{searchType});
  isnt($hunts->{data}->[4]->{searchType}, "", "should be missing searchType field");
  ok(exists $hunts->{data}->[4]->{query});
  isnt($hunts->{data}->[4]->{query}, undef, "should be missing query field");


# cleanup
  viewerDeleteToken("/hunt/$id5?molochRegressionUser=anonymous", $token);
  viewerDeleteToken("/hunt/$id6?molochRegressionUser=anonymous", $token);
  viewerDeleteToken("/hunt/$id7?molochRegressionUser=anonymous", $token);
  viewerDeleteToken("/hunt/$id8?molochRegressionUser=anonymous", $token);
  viewerPostToken("/user/views/delete?molochRegressionUser=user2", '{"expression":"protocols == tls","user":"user2","shared":true,"name":"tls"}', $otherToken);


# multiget should return an error
  my $mjson = multiGet("/hunt/list");
  is ($mjson->{text}, "Not supported in multies", "Hunt not supported in multies");


##  Now test hunts
  my (%HUNTS, %RESULTS);


  # Create 6 hunts based on the search type and search string
  sub createHunts {
    my ($stype, $str) = @_;

    $HUNTS{"raw-$stype-both-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"totalSessions":1,"name":"' . "raw-$stype-both-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}', $hToken);
    $HUNTS{"raw-$stype-src-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"totalSessions":1,"name":"' . "raw-$stype-src-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"raw","src":true,"dst":false,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}', $hToken);
    $HUNTS{"raw-$stype-dst-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"totalSessions":1,"name":"' . "raw-$stype-dst-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"raw","src":false,"dst":true,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}', $hToken);
    $HUNTS{"reassembled-$stype-both-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"totalSessions":1,"name":"' . "reassembled-$stype-both-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"reassembled","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}', $hToken);
    $HUNTS{"reassembled-$stype-src-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"totalSessions":1,"name":"' . "reassembled-$stype-src-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"reassembled","src":true,"dst":false,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}', $hToken);
    $HUNTS{"reassembled-$stype-dst-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"totalSessions":1,"name":"' . "reassembled-$stype-dst-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"reassembled","src":false,"dst":true,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}', $hToken);
  }

  # Check hunt vars given name and what the match count should be
  sub checkHunt {
      my ($name, $match, $checkResults) = @_;
      my $id = $HUNTS{$name}->{hunt}->{id};
      my $hname = $HUNTS{$name}->{hunt}->{name};
      my $result = $RESULTS{$id};
      is ($result->{status}, 'finished', "$name finished check");
      is ($result->{searchedSessions}, 1, "$name searchedSessions check");
      is ($result->{totalSessions}, 1, "$name totalSessions check");
      is ($result->{matchedSessions}, $match, "$name match check");

      if ($checkResults) {
          countTest($match, "date=-1&expression=" . uri_escape("huntId=$id"));
          countTest($match, "date=-1&expression=" . uri_escape("huntName=$hname"));
      }
  }


  createHunts("ascii", "Get");
  createHunts("ascii", "Gif");

  createHunts("asciicase", "Get");
  createHunts("asciicase", "Gif");

  createHunts("asciicase", "GET");

  createHunts("regex", "G..89");

  createHunts("hex", "766d663d");
  createHunts("hexregex", "766..63d");

  # create a hunt for regex dos
  $HUNTS{"raw-regex-both-(.*a){25}x"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"totalSessions":67,"name":"' . "raw-regex-both-(.*a){25}x-$$" . '", "size":"50","search":"(.*a){25}x","searchType":"regex","type":"raw","src":true,"dst":true,"query":{"startTime":1430916462,"stopTime":1569170858,"expression":"tags != bdat*"}}', $hToken);

  # Actually process the hunts
  viewerGet("/regressionTests/processHuntJobs");

  # create hash of results
  $hunts = viewerGet("/hunt/list?history=true");
  foreach my $item (@{$hunts->{data}}) {
      $RESULTS{$item->{id}} = $item;
  }

  # Check results
  checkHunt("raw-ascii-both-Get", 1, 1);
  checkHunt("raw-ascii-src-Get", 1, 1);
  checkHunt("raw-ascii-dst-Get", 0, 1);
  checkHunt("reassembled-ascii-both-Get", 1, 1);
  checkHunt("reassembled-ascii-src-Get", 1, 1);
  checkHunt("reassembled-ascii-dst-Get", 0, 1);

  checkHunt("raw-ascii-both-Gif", 1);
  checkHunt("raw-ascii-src-Gif", 0);
  checkHunt("raw-ascii-dst-Gif", 1);
  checkHunt("reassembled-ascii-both-Gif", 1);
  checkHunt("reassembled-ascii-src-Gif", 0);
  checkHunt("reassembled-ascii-dst-Gif", 1);

  checkHunt("raw-asciicase-both-Get", 0);
  checkHunt("raw-asciicase-src-Get", 0);
  checkHunt("raw-asciicase-dst-Get", 0);
  checkHunt("reassembled-asciicase-both-Get", 0);
  checkHunt("reassembled-asciicase-src-Get", 0);
  checkHunt("reassembled-asciicase-dst-Get", 0);

  checkHunt("raw-asciicase-both-Gif", 0);
  checkHunt("raw-asciicase-src-Gif", 0);
  checkHunt("raw-asciicase-dst-Gif", 0);
  checkHunt("reassembled-asciicase-both-Gif", 0);
  checkHunt("reassembled-asciicase-src-Gif", 0);
  checkHunt("reassembled-asciicase-dst-Gif", 0);

  checkHunt("raw-asciicase-both-GET", 1);
  checkHunt("raw-asciicase-src-GET", 1);
  checkHunt("raw-asciicase-dst-GET", 0);
  checkHunt("reassembled-asciicase-both-GET", 1);
  checkHunt("reassembled-asciicase-src-GET", 1);
  checkHunt("reassembled-asciicase-dst-GET", 0);

  checkHunt("raw-regex-both-G..89", 1);
  checkHunt("raw-regex-src-G..89", 0);
  checkHunt("raw-regex-dst-G..89", 1);
  checkHunt("reassembled-regex-both-G..89", 1);
  checkHunt("reassembled-regex-src-G..89", 0);
  checkHunt("reassembled-regex-dst-G..89", 1);

  checkHunt("raw-hex-both-766d663d", 1);
  checkHunt("raw-hex-src-766d663d", 1);
  checkHunt("raw-hex-dst-766d663d", 0);
  checkHunt("reassembled-hex-both-766d663d", 1);
  checkHunt("reassembled-hex-src-766d663d", 1);
  checkHunt("reassembled-hex-dst-766d663d", 0);

  checkHunt("raw-hexregex-both-766..63d", 1);
  checkHunt("raw-hexregex-src-766..63d", 1);
  checkHunt("raw-hexregex-dst-766..63d", 0);
  checkHunt("reassembled-hexregex-both-766..63d", 1);
  checkHunt("reassembled-hexregex-src-766..63d", 1);
  checkHunt("reassembled-hexregex-dst-766..63d", 0);

  # check results for regex dos
  my $id = $HUNTS{"raw-regex-both-(.*a){25}x"}->{hunt}->{id};
  my $result = $RESULTS{$id};
  is ($result->{status}, 'finished', "raw-regex-both-(.*a){25}x finished check");
  is ($result->{searchedSessions}, 68, "raw-regex-both-(.*a){25}x searchedSessions check");
  is ($result->{totalSessions}, 68, "raw-regex-both-(.*a){25}x totalSessions check");
  is ($result->{matchedSessions}, 0, "raw-regex-both-(.*a){25}x match check");

# cleanup
  $json = viewerPostToken("/user/delete", "userId=huntuser", $token);
  viewerDeleteToken("/hunt/$id1?molochRegressionUser=anonymous", $token);
  viewerDeleteToken("/hunt/$id3?molochRegressionUser=anonymous", $token);
  esPost("/tests_hunts/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
  viewerDeleteToken("/api/view/${viewId}?molochRegressionUser=user2", $otherToken);
