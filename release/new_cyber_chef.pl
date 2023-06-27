#!/usr/bin/perl
# This script will update the viewer/public/cyberchef.html and viewer/Makefile.in file for
# a new cyberchef release. It will also launch CHANGELOG to be edited and print out the
# git command to use after testing.

use strict;

die "$0 <new version>" if ($#ARGV != 0);

my $VERSION = $ARGV[0];

chdir "../viewer/public";

system "wget -N https://github.com/gchq/CyberChef/releases/download/v$VERSION/CyberChef_v$VERSION.zip";
system "unzip -o CyberChef_v$VERSION.zip CyberChef_v$VERSION.html";

open my $fh, '<', "CyberChef_v$VERSION.html" or die "Can't open file $!";
my $html = do { local $/; <$fh> };
close($fh);

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
        interval = setInterval(() => {
          if (typeof app !== 'undefined') {
            app.manager.recipe.addOperation('From Hex');
            app.setInput(result.data);
            clearInterval(interval);
          }
        }, 100);
      })
      .catch((error) => {
        console.log('error', error);
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

open my $fh, '>', "cyberchef.html" or die "Can't open file $!";
print $fh $html;
print $fh "\n";
close $fh;

unlink "CyberChef_v$VERSION.html";

system "perl -pi -e 's/v.*\\/CyberChef_v.*.zip/v$VERSION\\/CyberChef_v$VERSION.zip/g' ../Makefile.in";
system qq{perl -pi -e "s/CYBERCHEFVERSION.*,/CYBERCHEFVERSION: '$VERSION',/g" ../internals.js};

system "vim ../../CHANGELOG";
print qq{When ready do "git commit -m 'Updated to cyberchef $VERSION' CHANGELOG viewer/Makefile.in viewer/public/cyberchef.html viewer/internals.js"\n}
