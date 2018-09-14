use Test::More tests => 22;
use Cwd;
use URI::Escape;
use MolochTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $token = getTokenCookie();
my $otherToken = getTokenCookie('user2');

# Must have token to add a hunt
  my $json = viewerPost("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}}');
  my $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without a token");

# Must apply to sessions to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":0,"name":"test hunt","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job that doesn't apply to sessions");

# Must have a name to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without a name");

# Must have a size to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without a size");

# Must have search text to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","searchType":"ascii","type":"raw","src":true,"dst":true}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without search text");

# Must have search text type to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","type":"raw","src":true,"dst":true}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without search text type");

# Must have a valid search text type to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","searchType":"asdf","type":"raw","src":true,"dst":true}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without search text type");

# Must have a type to add a hunt
  $json = viewerPostToken("/hunt",'{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","searchType":"ascii","src":true,"dst":true}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without a type");

# Must have a valid type to add a hunt
  $json = viewerPostToken("/hunt",'{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","searchType":"ascii","type":"asdf","src":true,"dst":true}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without a type");

# Must have src or dst to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","searchType":"ascii","type":"raw"}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without a type");

# Must have query to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without a query");

# Must have fully formed query to add a hunt
  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000}}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without a query stopTime");

  $json = viewerPostToken("/hunt", '{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"stopTime":1536872891}}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 0, "Can't add a job without a query starTime");

# Add a valid hunt
  $json = viewerPostToken("/hunt?molochRegressionUser=anonymous", '{"hunt":{"totalSessions":1,"name":"test hunt~`!@#$%^&*()[]{};<>?/`","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}}', $token);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 1, "Add hunt 1");

# Make sure the hunt's name doesn't contain special chars
  is ($json->{hunt}->{name}, "test hunt", "Strip special chars");

# If the user is not an admin they can only delete their own hunts
  my $id1 = $json->{hunt}->{id};
  $json = viewerDeleteToken("/hunt/$id1?molochRegressionUser=user2", $otherToken);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 1, "Non admin user cannot delete another user's hunt");

  $json = viewerPostToken("/hunt?molochRegressionUser=user2", '{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}}', $otherToken);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 2, "Add hunt 2");

  my $id2 = $json->{hunt}->{id};
  $json = viewerDeleteToken("/hunt/$id2?molochRegressionUser=user2", $otherToken);
  $hunts = viewerGet("/hunt/list", "");
  is (@{$hunts->{data}}, 1, "User can remove their own hunt");

# If the user is not an admin they can only pause their own hunts
  $json = viewerPostToken("/hunt?molochRegressionUser=anonymous", '{"hunt":{"totalSessions":1,"name":"test hunt~`!@#$%^&*()[]{};<>?/`","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}}', $token);
  my $id3 = $json->{hunt}->{id};
  $json = viewerPutToken("/hunt/$id3/pause?molochRegressionUser=user2", $otherToken);
  is ($json->{text}, "You cannot change another user\'s hunt unless you have admin privelages", "Non admin user cannot pause another user's hunt");

# If the user is not an admin they can only play their own hunts
  $json = viewerPutToken("/hunt/$id3/play?molochRegressionUser=user2", $otherToken);
  is ($json->{text}, "You cannot change another user\'s hunt unless you have admin privelages", "Non admin user cannot pause another user's hunt");

# Admin can delete any hunt
  $json = viewerPostToken("/hunt?molochRegressionUser=user2", '{"hunt":{"totalSessions":1,"name":"test hunt","size":"50","search":"test search text","searchType":"ascii","type":"raw","src":true,"dst":true,"query":{"startTime":18000,"stopTime":1536872891}}}', $otherToken);
  my $id4 = $json->{hunt}->{id};
  $json = viewerDeleteToken("/hunt/$id4?molochRegressionUser=anonymous", $token);
  is (@{$hunts->{data}}, 1, "Admin can remove any hunt");

# multiget should return an error
  my $mjson = multiGet("/hunt/list");
  is ($mjson->{text}, "Not supported in multies", "Hunt not supported in multies");

# cleanup
  viewerDeleteToken("/hunt/$id1?molochRegressionUser=anonymous", $token);
  viewerDeleteToken("/hunt/$id3?molochRegressionUser=anonymous", $token);
