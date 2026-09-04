use Test::More tests => 35;
use Cwd;
use URI::Escape;
use ArkimeTest;
use JSON;
use Test::Differences;
use Data::Dumper;
use strict;

my $token = getTokenCookie();
my $notAdminToken = getTokenCookie('banner-notadmin');

# add a non-admin user for the permission test
  viewerPostToken("/api/user", '{"userId": "banner-notadmin", "userName": "notadmin", "enabled":true, "password":"password", "roles":["arkimeUser"]}', $token);

# default banner is disabled/empty
  my $banner = viewerGet("/api/banner");
  ok(!$banner->{enabled}, "default banner disabled");
  is($banner->{message}, "", "default banner empty message");
  is($banner->{type}, "info", "default banner type info");

# update banner requires token and admin access
  my $json = viewerPut("/api/banner", '{"enabled":true,"message":"hi","type":"info"}');
  is($json->{text}, "Missing token", "update banner requires token");
  $json = viewerPutToken("/api/banner?arkimeRegressionUser=banner-notadmin", '{"enabled":true,"message":"hi","type":"info"}', $notAdminToken);
  is($json->{text}, "You do not have permission to access this resource", "update banner requires admin");

# validation
  $json = viewerPutToken("/api/banner", '{"enabled":true,"message":"hi","type":"bogus"}', $token);
  is($json->{text}, "Unknown banner type", "invalid banner type caught");
  $json = viewerPutToken("/api/banner", '{"enabled":true,"message":123,"type":"info"}', $token);
  is($json->{text}, "Banner message must be a string", "non-string message caught");
  $json = viewerPutToken("/api/banner", '{"enabled":true,"message":"' . ('x' x 1001) . '","type":"info"}', $token);
  is($json->{text}, "Banner message must be 1000 characters or less", "too-long message caught");

# update banner
  $json = viewerPutToken("/api/banner", '{"enabled":true,"message":"down tomorrow","type":"warning"}', $token);
  ok($json->{success}, "banner update success");
  is($json->{banner}->{message}, "down tomorrow", "banner message saved");
  is($json->{banner}->{type}, "warning", "banner type saved");
  ok($json->{banner}->{enabled}, "banner enabled saved");
  ok(exists $json->{banner}->{updated}, "banner updated field set");
  is($json->{banner}->{user}, "anonymous", "banner user field set");

# get reflects the saved banner
  $banner = viewerGet("/api/banner");
  is($banner->{message}, "down tomorrow", "get returns saved message");
  is($banner->{type}, "warning", "get returns saved type");
  ok($banner->{enabled}, "get returns enabled");
  ok(!exists $banner->{user}, "get does not leak the editing user");

# effects + expires validation
  $json = viewerPutToken("/api/banner", '{"enabled":true,"message":"x","type":"info","effects":["disco"]}', $token);
  is($json->{text}, "Unknown banner effect", "invalid effect caught");
  $json = viewerPutToken("/api/banner", '{"enabled":true,"message":"x","type":"info","effects":"marquee"}', $token);
  is($json->{text}, "Banner effects must be an array", "non-array effects caught");
  $json = viewerPutToken("/api/banner", '{"enabled":true,"message":"x","type":"info","expires":"soon"}', $token);
  is($json->{text}, "Banner expires must be a number", "invalid expires caught");

# effects + expires saved and reflected
  $json = viewerPutToken("/api/banner", '{"enabled":true,"message":"ticker","type":"info","effects":["marquee","rainbow"],"expires":9999999999999}', $token);
  ok($json->{success}, "banner with effects+expires saved");
  is($json->{banner}->{effects}->[0], "marquee", "first effect saved");
  is($json->{banner}->{effects}->[1], "rainbow", "second effect saved");
  is($json->{banner}->{expires}, 9999999999999, "expires saved");
  $banner = viewerGet("/api/banner");
  is($banner->{effects}->[0], "marquee", "get returns effects");

# mass assignment - extra fields should not be stored
  $json = viewerPutToken("/api/banner", '{"enabled":true,"message":"clean","type":"info","evil":"injected"}', $token);
  ok($json->{success}, "banner with extra field updates ok");
  $banner = viewerGet("/api/banner");
  ok(!exists $banner->{evil}, "extra field not stored in banner");

# sync requires token + admin
  $json = viewerPost("/api/banner/sync", "{}");
  is($json->{text}, "Missing token", "sync requires token");
  $json = viewerPostToken("/api/banner/sync?arkimeRegressionUser=banner-notadmin", "{}", $notAdminToken);
  is($json->{text}, "You do not have permission to access this resource", "sync requires admin");

# sync copies this app's banner to all apps
  $json = viewerPutToken("/api/banner", '{"enabled":true,"message":"syncme","type":"info"}', $token);
  ok($json->{success}, "banner set before sync");
  $json = viewerPostToken("/api/banner/sync", "{}", $token);
  ok($json->{success}, "banner sync success");
  is($json->{banner}->{message}, "syncme", "sync returns the synced banner");

# cleanup - disable + sync so no app is left with a banner
  $json = viewerPutToken("/api/banner", '{"enabled":false,"message":"","type":"info"}', $token);
  ok($json->{success}, "banner disable success");
  viewerPostToken("/api/banner/sync", "{}", $token);
  $banner = viewerGet("/api/banner");
  ok(!$banner->{enabled}, "banner disabled after cleanup");

# remove added user
  viewerDeleteToken("/api/user/banner-notadmin", $token);
