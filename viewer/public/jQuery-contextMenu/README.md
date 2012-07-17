# jQuery contextMenu plugin & polyfill #

$.contextMenu is a management facility for - you guessed it - context menus. It was designed for an application where there are hundreds of elements that may show a context menu - so intialization speed and memory usage are kept fairly small. It also allows to register context menus without providing actual markup, as $.contextMenu generates DOMElements as needed.

[features](http://medialize.github.com/jQuery-contextMenu/index.html) - 
[demo](http://medialize.github.com/jQuery-contextMenu/demo.html) - 
[documentation](http://medialize.github.com/jQuery-contextMenu/docs.html)


## Dependencies ##

* jQuery 1.7 (using new .on().off() event API)
* jQuery UI position (optional but recommended)

## Usage ##

register contextMenu from javascript:

```javascript
$.contextMenu({
    // define which elements trigger this menu
    selector: ".with-cool-menu",
    // define the elements of the menu
    items: {
        foo: {name: "Foo", callback: function(key, opt){ alert("Foo!"); }},
        bar: {name: "Bar", callback: function(key, opt){ alert("Bar!") }}
    }
    // there's more, have a look at the demos and docs...
});
```

have a look at the [demos](http://medialize.github.com/jQuery-contextMenu/demo.html).


## HTML5 Compatibility ##

Firefox 8 implemented contextmenu using the &lt;menuitem&gt; tags for menu-structure. The specs however state that &lt;command&gt; tags should be used for this purpose. $.contextMenu accepts both.

Firefox 8 does not yet fully implement the contextmenu specification ([Ticket #617528](https://bugzilla.mozilla.org/show_bug.cgi?id=617528)). The elements
[a](http://www.whatwg.org/specs/web-apps/current-work/multipage/commands.html#using-the-a-element-to-define-a-command),
[button](http://www.whatwg.org/specs/web-apps/current-work/multipage/commands.html#using-the-button-element-to-define-a-command),
[input](http://www.whatwg.org/specs/web-apps/current-work/multipage/commands.html#using-the-input-element-to-define-a-command) and
[option](http://www.whatwg.org/specs/web-apps/current-work/multipage/commands.html#using-the-option-element-to-define-a-command) 
usable as commands are being ignored altogether. It also doesn't (optically) distinguish between checkbox/radio and regular commands ([Bug #705292](https://bugzilla.mozilla.org/show_bug.cgi?id=705292)).

* [contextmenu specs](http://www.w3.org/TR/html5/interactive-elements.html#context-menus)
* [command specs](http://www.whatwg.org/specs/web-apps/current-work/multipage/commands.html)
* [Browser support according to caniuse.com](http://caniuse.com/#search=context%20menu)

Note: While the specs note &lt;option&gt;s to be renderd as regular commands, $.contextMenu will render an actual &lt;select&gt;. import contextMenu from HTML5 &lt;menu&gt;:

```javascript
$.contextMenu("html5");
```

## Interaction Principles ##

You're (obviously) able to use the context menu with your mouse. Once it is opened, you can also use the keyboard to (fully) navigate it.

* ↑ (up) previous item in list, will skip disabled elements and wrap around
* ↓ (down) next item in, will skip disabled elements and wrap around
* → (right) dive into sub-menu
* ← (left) rise from sub-menu
* ↵ (return) invoke command
* ⇥ (tab) next item or input element, will skip disabled elements and wrap around
* ⇪ ⇥ (shift tab) previous item or input element, will skip disabled elements and wrap around
* ⎋ (escape) close menu
* ⌴ (space) captured and ignore to avoid page scrolling (for consistency with native menus)
* ⇞ (page up) captured and ignore to avoid page scrolling (for consistency with native menus)
* ⇟ (page down) captured and ignore to avoid page scrolling (for consistency with native menus)
* ↖ (home) first item in list, will skip disabled elements
* ↘ (end) last item in, will skip disabled elements

Besides the obvious, browser also react to alphanumeric key strokes. Hitting <code>r</code> in a context menu will make Firefox (8) reload the page immediately. Chrome selects the option to see infos on the page, Safari selects the option to print the document. Awesome, right? Until trying the same on Windows I did not realize that the browsers were using the access-key for this. I would've preferred typing the first character of something, say "s" for "save" and then iterate through all the commands beginning with s. But that's me - what do I know about UX? Anyways, $.contextMenu now also supports accesskey handling.


## Minify ##

use [Google Closure Compiler](http://closure-compiler.appspot.com/home):

```
// ==ClosureCompiler==
// @compilation_level SIMPLE_OPTIMIZATIONS
// @output_file_name contextMenu.js
// @code_url http://medialize.github.com/jQuery-contextMenu/jquery-1.7.1.min.js
// @code_url http://medialize.github.com/jQuery-contextMenu/jquery.ui.position.js
// @code_url http://medialize.github.com/jQuery-contextMenu/jquery.contextMenu.js
// ==/ClosureCompiler==    
```


## Authors ##

* [Rodney Rehm](https://github.com/rodneyrehm)
* [Christiaan Baartse](https://github.com/christiaan) (single callback per menu)
* [Addy Osmani](https://github.com/addyosmani) (compatibility with native context menu in Firefox 8)


## License ##

$.contextMenu is published under the [MIT license](http://www.opensource.org/licenses/mit-license) and [GPL v3](http://opensource.org/licenses/GPL-3.0).


## Changelog ##

### 1.5.20 ###

* Fixing backdrop would not position properly in IE6 (Issue #59)
* Fixing nested input elements not accessible in Chrome / Safari (Issue #58)

### 1.5.19 ###

* fixing sub-menu positioning when `$.ui.position` is not available (Issue #56)

### 1.5.18 ###

* fixing html5 `<menu>` import (Issue #53)

### 1.5.17 ###

* fixing `options` to default to `options.trigger = "right"`
* fixing variable name typo (Within Issue #51)
* fixing menu not closing while opening other menu (Within Issue #51)
* adding workaround for `contextmenu`-bug in Firefox 12 (Within Issue #51)

### 1.5.16 ###

* added vendor-prefixed user-select to CSS
* fixed issue with z-indexing when `<body>` is used as a trigger (Issue #49)

### 1.5.15 ###

* allowing to directly open another element's menu while a menu is shown (Issue #48)
* fixing autohide option that would not properly hide the menu

### 1.5.14 ###

* options.build() would break default options (Issue #47)
* $.contextMenu('destroy') would not remove backdrop

### 1.5.13 ###

* exposing $trigger to dynamically built custom menu-item types (Issue #42)
* fixing repositioning of open menu (formerly accidental re-open)
* adding asynchronous example
* dropping ignoreRightClick in favor of proper event-type detection

### 1.5.12 ###

* prevent invoking callback of first item of a sub-menu when clicking on the sub-menu-item (Issue #41)

### 1.5.11 ###

* providing `opt.$trigger` to show event (Issue #39)

### 1.5.10 ###

* ignoreRightClick would not prevent right click when menu is already open (Issue #38)

### 1.5.9 ###

* If build() did not return any items, an empty menu was shown (Issue #33)

### 1.5.8 ###

* Capturing Page Up and Page Down keys to ignore like space (Issue #30)
* Added Home / End keys to jump to first / last command of menu (Issue #29)
* Bug hitting enter in an &lt;input&gt; would yield an error (Issue #28)

### 1.5.7 ###

* Non-ASCII character in jquery.contextMenu.js caused compatibility issues in Rails (Issue #27)

### 1.5.6 ###

* Bug contextmenu event was not passed to build() callback (Issue #24)
* Bug sub-menu markers would not display properly in Safari and Chrome (Issue #25)

### 1.5.5 ###

* Bug Internet Explorer would not close menu when giving input elements focus (Issue #23)

### 1.5.4 ###

* Bug not set z-index of sub-menus might not overlap the main menu correctly (Issue #22)

### 1.5.3 ###

* Bug `console.log is undefined`

### 1.5.2 ###

* Bug sub-menus would not properly update their disabled states (Issue #16) [again…]
* Bug sub-menus would not properly adjust width accoring to min-width and max-width (Issue #18)

### 1.5.1 ###

* Bug sub-menus would not properly update their disabled states (Issue #16)

### 1.5 ###

* Added [dynamic menu creation](http://medialize.github.com/jQuery-contextMenu/demo/dynamic-create.html) (Issue #15)

### 1.4.4 ###

* Bug positioning &lt;menu&gt; when trigger element is `position:fixed` (Issue #14)

### 1.4.3 ###

* Bug key handler would caputure all key strokes while menu was visible (essentially disabling F5 and co.)

### 1.4.2 ###

* Bug opt.$trigger was not available to disabled callbacks
* jQuery bumped to 1.7.1

### 1.4.1 ###

* Bug where &lt;menu&gt; imports would not pass action (click event) properly

### 1.4 ###

* Upgraded to jQuery 1.7 (changed dependecy!)
* Added internal events `contextmenu:focus`, `contextmenu:blur` and `contextmenu:hide`
* Added custom &lt;command&gt; types
* Bug where `className` wasn't properly set on &lt;menu&gt;

### 1.3 ###

* Added support for accesskeys
* Bug where two sub-menus could be open simultaneously

### 1.2.2 ###

* Bug in HTML5 import

### 1.2.1 ###

* Bug in HTML5 detection

### 1.2 ###

* Added compatibility to &lt;menuitem&gt; for Firefox 8
* Upgraded to jQuery 1.6.2

### 1.1 ###

* Bug #1 TypeError on HTML5 action passthru
* Bug #2 disbaled callback not invoked properly
* Feature #3 auto-hide option for hover trigger
* Feature #4 option to use a single callback for all commands, rather than registering the same function for each item
* Option to ignore right-click (original "contextmenu" event trigger) for non-right-click triggers

### 1.0 ###

* Initial $.contextMenu handler