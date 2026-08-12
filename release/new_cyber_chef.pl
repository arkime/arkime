#!/usr/bin/perl
# This script will update the viewer/public/cyberchef.html and viewer/Makefile.in file for
# a new cyberchef release. It will also launch CHANGELOG to be edited and print out the
# git command to use after testing.
#
# The release zip is named CyberChef_<git sha>.zip, so the sha is looked up from the
# github release for the version. Optionally the sha can be passed in.

use strict;

die "$0 <new version> [<zip sha>]" if ($#ARGV < 0 || $#ARGV > 1);

my $VERSION = $ARGV[0];
my $SHA = $ARGV[1] // findSha($VERSION);

print "Using CyberChef $VERSION sha $SHA\n";

chdir "../viewer/public";

# Download using the sha based remote name, but save with the version based name that
# the Makefile/viewer expect
system "curl -sSfL -o CyberChef_v$VERSION.zip https://github.com/gchq/CyberChef/releases/download/v$VERSION/CyberChef_$SHA.zip" and die "download failed";
system "unzip -o CyberChef_v$VERSION.zip CyberChef_v$VERSION.html";

open my $fh, '<', "CyberChef_v$VERSION.html" or die "Can't open file $!";
my $html = do { local $/; <$fh> };
close($fh);

my $beforescript = q|
<script>
    let safehref = window.location.href.replace(/%3[cC]/g, '%26lt;');
    if (window.location.href !== safehref) {
      console.log("Hacker", window.location.href, safehref);
      window.location.href = safehref;
    }
</script>
|;

my $script = q|
  <script>
    let href = window.location.href;
    let search = href.split('?')[1];
    let params = search.split('&');
    let node, session, type = 'src';
    for (let param of params) {
      if (param.startsWith('node')) {
        node = param.split('=')[1];
      } else if (param.startsWith('session')) {
        session = param.split('=')[1];
      } else if (param.startsWith('type')) {
        type = param.split('=')[1];
      }
    }

    let data;
    let interval;

    // fetch the data to populate the input
    fetch(`${node}/session/${session}?type=${type}`)
      .then((response) => {
        if (response.ok) {
          return response.json();
        } else {
          throw new Error('Error retrieving data');
        }
      })
      .then((result) => {
        data = result.data;
      })
      .catch((error) => {
        console.log('error', error);
      }).finally(() => {
        interval = setInterval(() => {
          if (typeof app !== 'undefined') {
            if (data) {
              app.manager.recipe.addOperation('From Hex');
              app.setInput(data);
            }

            clearInterval(interval);
          }
        }, 100);
      });

    setTimeout(() => {
      // reset the route params because cyberchef removes them
      // so a user can reload the page
      window.history.replaceState({ id: 'CyberChef' }, 'CyberChef', href);
    }, 2000);
  </script>
|;


$html =~ s|<head>|<head>\n<base href="./cyberchef/" /><meta name="referrer" content="no-referrer">\n|;
$html =~ s|</body>|$script</body>|;

open my $out, '>', "cyberchef.html" or die "Can't open file $!";
print $out $beforescript;
print $out $html;
print $out "\n";
close $out;

unlink "CyberChef_v$VERSION.html";

updateMakefile("../Makefile.in", $VERSION, $SHA);
system qq{perl -pi -e "s/CYBERCHEFVERSION.*,/CYBERCHEFVERSION: '$VERSION',/g" ../internals.js};

system "vim ../../CHANGELOG";
print qq{When ready do "git commit -m 'Updated to cyberchef $VERSION' CHANGELOG viewer/Makefile.in viewer/public/cyberchef.html viewer/internals.js"\n};

################################################################################
# Find the sha in the zip asset name of the github release for this version
sub findSha {
    my ($version) = @_;

    my $url = "https://api.github.com/repos/gchq/CyberChef/releases/tags/v$version";
    my $json = `curl -sSfL -H "Accept: application/vnd.github+json" $url`;
    die "Couldn't fetch $url" if ($? != 0 || !$json);

    my ($sha) = $json =~ /"name"\s*:\s*"CyberChef_([0-9a-f]{6,64})\.zip"/;
    die "Couldn't find CyberChef_<sha>.zip asset for v$version, pass the sha as the 2nd argument" if (!$sha);

    return $sha;
}
################################################################################
# Update, or add if missing, the CYBERCHEF_VERSION and CYBERCHEF_SHA variables
sub updateMakefile {
    my ($file, $version, $sha) = @_;

    open my $in, '<', $file or die "Can't open $file $!";
    my $data = do { local $/; <$in> };
    close($in);

    if ($data =~ s/^CYBERCHEF_VERSION=.*$/CYBERCHEF_VERSION=$version/m) {
        if (!($data =~ s/^CYBERCHEF_SHA=.*$/CYBERCHEF_SHA=$sha/m)) {
            $data =~ s/^(CYBERCHEF_VERSION=.*)$/$1\nCYBERCHEF_SHA=$sha/m;
        }
    } else {
        die "Couldn't find CYBERCHEF_VERSION in $file";
    }

    open my $out, '>', $file or die "Can't open $file $!";
    print $out $data;
    close($out);
}
