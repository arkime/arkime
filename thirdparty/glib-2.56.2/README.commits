GLib is part of the GNOME git repository. At the current time, any
person with write access to the GNOME repository, can make changes to
GLib. This is a good thing, in that it encourages many people to work
on GLib, and progress can be made quickly. However, GLib is a fairly
large and complicated package that many other things depend on, so to
avoid unnecessary breakage, and to take advantage of the knowledge
about GLib that has been built up over the years, we'd like to ask
people committing to GLib to follow a few rules:

0) Ask first. If your changes are major, or could possibly break existing
   code, you should always ask. If your change is minor and you've
   been working on GLib for a while it probably isn't necessary
   to ask. But when in doubt, ask. Even if your change is correct,
   somebody may know a better way to do things.

   If you are making changes to GLib, you should be subscribed
   to gtk-devel-list@gnome.org. (Subscription address:
   gtk-devel-list-request@gnome.org.) This is a good place to ask
   about intended changes.

   #gtk+ on GIMPNet (irc.gimp.org, irc.us.gimp.org, irc.eu.gimp.org, ...)
   is also a good place to find GTK+ developers to discuss changes with,
   however, email to gtk-devel-list is the most certain and preferred
   method.

1) Ask _first_.

2) With git, we no longer maintain a ChangeLog file, but you are expected
   to produce a meaningful commit message. Changes without a sufficient
   commit message will be reverted. See below for the expected format
   of commit messages.

Notes:

* When developing larger features or complicated bug fixes, it is
  advisable to work in a branch in your own cloned GLib repository.
  You may even consider making your repository publically available
  so that others can easily test and review your changes.

* The expected format for git commit messages is as follows:

=== begin example commit ===
Short explanation of the commit

Longer explanation explaining exactly what's changed, whether any
external or private interfaces changed, what bugs were fixed (with bug
tracker reference if applicable) and so forth. Be concise but not too brief.
=== end example commit ===

  - Always add a brief description of the commit to the _first_ line of
    the commit and terminate by two newlines (it will work without the
    second newline, but that is not nice for the interfaces).

  - First line (the brief description) must only be one sentence and
    should start with a capital letter unless it starts with a lowercase
    symbol or identifier. Don't use a trailing period either. Don't exceed
    72 characters.

  - The main description (the body) is normal prose and should use normal
    punctuation and capital letters where appropriate. Normally, for patches
    sent to a mailing list it's copied from there.

  - When committing code on behalf of others use the --author option, e.g.
    git commit -a --author "Joe Coder <joe@coder.org>" and --signoff.


Owen Taylor
13 Aug 1998
17 Apr 2001

Matthias Clasen
31 Mar 2009
