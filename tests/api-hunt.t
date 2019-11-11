use Test::More tests => 250;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $token = getTokenCookie();
my $otherToken = getTokenCookie('user2');
my $json;


# Delete old hunts
  esPost("/tests_hunts/hunt/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');

# Make sure no hunts
  my $hunts = viewerGet("/hunt/list");
  eq_or_diff($hunts, from_json('{"recordsTotal": 0, "data": [], "recordsFiltered": 0}'));

# Create huntuser
  $json = viewerPostToken("/user/create", '{"userId": "huntuser", "userName": "UserName", "enabled":true, "password":"password", "packetSearch":true}', $token);

my $hToken = getTokenCookie('huntuser');

##### ERRORS
# Must have token to add a hunt
  $json = viewerPost("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt 1","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}}');
  eq_or_diff($json, from_json('{"text": "Missing token", "success": false}'));

# Must apply to sessions to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":0,"name":"test hunt 2","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}}', $token);
  eq_or_diff($json, from_json('{"text": "This hunt does not apply to any sessions", "success": false}'));

# Must have a name to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing hunt name", "success": false}'));

# Must have a size to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt 3","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing max mumber of packets to examine per session", "success": false}'));

# Must have search text to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt 4","size":"50","searchType":"ascii","type":"raw","src":true,"dst":true}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing packet search text", "success": false}'));

# Must have search text type to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt 5","size":"50","search":"test search text","type":"raw","src":true,"dst":true, "query": {"startTime":0, "stopTime":1}}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing packet search text type", "success": false}'));

# Must have a valid search text type to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt 6","size":"50","search":"test search text","searchType":"asdf","type":"raw","src":true,"dst":true, "query": {"startTime":0, "stopTime":1}}}', $token);
  eq_or_diff($json, from_json('{"text": "Improper packet search text type. Must be \"ascii\", \"asciicase\", \"hex\", \"wildcard\", \"hexregex\", or \"regex\"", "success": false}'));

# Must have a type to add a hunt
  $json = viewerPostToken("/hunt",'{"hunt":{"totalSessions":1,"name":"test hunt 7","size":"50","search":"test search text","searchType":"ascii","src":true,"dst":true, "query": {"startTime":0, "stopTime":1}}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing packet search type (raw or reassembled packets)", "success": false}'));

# Must have a valid type to add a hunt
  $json = viewerPostToken("/hunt",'{"hunt":{"totalSessions":1,"name":"test hunt 8","size":"50","search":"test search text","searchType":"ascii","type":"asdf","src":true,"dst":true, "query": {"startTime":0, "stopTime":1}}}', $token);
  eq_or_diff($json, from_json('{"text": "Improper packet search type. Must be \"raw\" or \"reassembled\"", "success": false}'));

# Must have src or dst to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt 9","size":"50","search":"test search text","searchType":"ascii","type":"raw"}}', $token);
  eq_or_diff($json, from_json('{"text": "The hunt must search source or destination packets (or both)", "success": false}'));

# Must have query to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt 10","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing query", "success": false}'));

# Must have fully formed query to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt 11","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000}}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing fully formed query (must include start time and stop time)", "success": false}'));

  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt 12","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"stopTime":1536872891}}}', $token);
  eq_or_diff($json, from_json('{"text": "Missing fully formed query (must include start time and stop time)", "success": false}'));

# Make sure no hunts
  my $hunts = viewerGet("/hunt/list");
  eq_or_diff($hunts, from_json('{"recordsTotal": 0, "data": [], "recordsFiltered": 0}'));

##### GOOD

# Add a valid hunt, and it should immediately run
  $json = viewerPostToken("/hunt?molochRegressionUser=anonymous", '{"hunt":{"totalSessions":1,"name":"test hunt 13~`!@#$%^&*()[]{};<>?/`","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}}', $token);
  is ($json->{success}, 1);

# Make sure the hunt's name doesn't contain special chars
  is ($json->{hunt}->{name}, "test hunt 13", "Strip special chars");

# Hunt should finish
  viewerGet("/processHuntJobs");

  $hunts = viewerGet("/hunt/list?molochRegressionUser=user2");
  is (@{$hunts->{data}}, 1, "Add hunt 1");

# user2 shouldn't see is, query, search, searchType, userId
  my $item = $hunts->{data}->[0];
  is($item->{id}, '');
  is($item->{search}, '');
  is($item->{searchType}, '');
  is($item->{userId}, '');
  ok(! exists $item->{query});

# If the user is not an admin they can only delete their own hunts
  my $id1 = $json->{hunt}->{id};
  $json = viewerDeleteToken("/hunt/$id1?molochRegressionUser=user2", $otherToken);
  is ($json->{text}, "You cannot change another user's hunt unless you have admin privileges");

  $hunts = viewerGet("/hunt/list");
  is (@{$hunts->{data}}, 1, "Non admin user cannot delete another user's hunt");

  $json = viewerPostToken("/hunt?molochRegressionUser=user2", '{"hunt":{"totalSessions":1,"name":"test hunt 14","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}}', $otherToken);

  viewerGet("/processHuntJobs");

  $hunts = viewerGet("/hunt/list");
  is (@{$hunts->{data}}, 2, "Add hunt 2");

  my $id2 = $json->{hunt}->{id};
  $json = viewerDeleteToken("/hunt/$id2?molochRegressionUser=user2", $otherToken);
  is ($json->{text}, "Deleted hunt item successfully");

  $hunts = viewerGet("/hunt/list");
  is (@{$hunts->{data}}, 1, "User can remove their own hunt");

# If the user is not an admin they can only pause their own hunts
  $json = viewerPostToken("/hunt?molochRegressionUser=anonymous", '{"hunt":{"totalSessions":1,"name":"test hunt 15~`!@#$%^&*()[]{};<>?/`","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}}', $token);
  my $id3 = $json->{hunt}->{id};

  $json = viewerPutToken("/hunt/$id3/pause?molochRegressionUser=user2", "{}", $otherToken);
  is ($json->{text}, "You cannot change another user\'s hunt unless you have admin privileges", "Non admin user cannot pause another user's hunt");

# If the user is not an admin they can only play their own hunts
  $json = viewerPutToken("/hunt/$id3/play?molochRegressionUser=user2", "{}", $otherToken);
  is ($json->{text}, "You cannot change another user\'s hunt unless you have admin privileges", "Non admin user cannot pause another user's hunt");

  $json = viewerDeleteToken("/hunt/$id3?molochRegressionUser=anonymous", $token);
  is ($json->{text}, "Deleted hunt item successfully");

# Admin can delete any hunt
  $json = viewerPostToken("/hunt?molochRegressionUser=user2", '{"hunt":{"totalSessions":1,"name":"test hunt 16","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}}', $otherToken);
  my $id4 = $json->{hunt}->{id};

  sleep(2); # Wait for it to finish processing
  $json = viewerDeleteToken("/hunt/$id4?molochRegressionUser=anonymous", $token);
  is ($json->{text}, "Deleted hunt item successfully");

  $hunts = viewerGet("/hunt/list");
  is (@{$hunts->{data}}, 1, "Admin can remove any hunt");

# multiget should return an error
  my $mjson = multiGet("/hunt/list");
  is ($mjson->{text}, "Not supported in multies", "Hunt not supported in multies");


##  Now test hunts
  my (%HUNTS, %RESULTS);


  # Create 6 hunts based on the search type and search string
  sub createHunts {
    my ($stype, $str) = @_;

    $HUNTS{"raw-$stype-both-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"hunt":{"totalSessions":1,"name":"' . "raw-$stype-both-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}}', $hToken);
    $HUNTS{"raw-$stype-src-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"hunt":{"totalSessions":1,"name":"' . "raw-$stype-src-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"raw","src":true,"dst":false,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}}', $hToken);
    $HUNTS{"raw-$stype-dst-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"hunt":{"totalSessions":1,"name":"' . "raw-$stype-dst-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"raw","src":false,"dst":true,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}}', $hToken);
    $HUNTS{"reassembled-$stype-both-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"hunt":{"totalSessions":1,"name":"' . "reassembled-$stype-both-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"reassembled","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}}', $hToken);
    $HUNTS{"reassembled-$stype-src-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"hunt":{"totalSessions":1,"name":"' . "reassembled-$stype-src-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"reassembled","src":true,"dst":false,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}}', $hToken);
    $HUNTS{"reassembled-$stype-dst-$str"} = viewerPostToken("/hunt?molochRegressionUser=huntuser", '{"hunt":{"totalSessions":1,"name":"' . "reassembled-$stype-dst-$str-$$" . '", "size":"50","search":"' . $str . '","searchType":"' . $stype . '","type":"reassembled","src":false,"dst":true,"query":{"startTime":18000,"stopTime":1536872891, "expression": "file == *http-wrapped-header.pcap"}}}', $hToken);
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

  # Actually process the hunts
  viewerGet("/processHuntJobs");

  esGet("/_flush");
  esGet("/_refresh");

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


# cleanup
  $json = viewerPostToken("/user/delete", "userId=huntuser", $token);
  viewerDeleteToken("/hunt/$id1?molochRegressionUser=anonymous", $token);
  viewerDeleteToken("/hunt/$id3?molochRegressionUser=anonymous", $token);
  #  esPost("/tests_hunts/hunt/_delete_by_query?conflicts=proceed&refresh", '{ "query": { "match_all": {} } }');
