# patch-locale-archive

![](https://raw.githubusercontent.com/mb3h/patch-locale-archive/master/inspect.png)
![](https://raw.githubusercontent.com/mb3h/patch-locale-archive/master/patching.png)
![](https://raw.githubusercontent.com/mb3h/patch-locale-archive/master/wcwidth-test.png)

## Design Policy

maintenance model (code paradigm): objective [C99,Node.js]


## Overview

This utility can be available to correct unsuitable wide-character
behavior on application which depends on 'wcwidth()' function.
(bash etc.)

For example, in operating 'bash' shell through SSH remote,
there is a case to erase only half of wide-charactor by
Back-space key.
This is the result of coding that wide-character judge of 'bash'
depends on 'wcwidth()' and it returned not 2 but 1.
'wcwidth()' refer #12 table of 'LC_CTYPE' on /usr/lib/locale/locale-archive,
so that you can correct to suitable behavior by changing this table's value.

Formally, /usr/lib/locale/locale-archive is created by 'locale-gen' or 'localedef'.
But these tools need a definition file (/usr/share/i18n/locales/XXX etc.)
for its execution, so that nobody can use these tools without its definition file.
(even if fixes are very slight)
On only situation which target locale information exists in 'locale-archve',
this utility can correct without the definition file.


## TODO priority / progress

TODO
- [x] append node.js version.
- [ ] complete package.json (node.js version)
- [ ] append level3 table dump option about specified characters.

PENDING
- [ ] SUMHASH index collision support.
- [ ] U+10000..U+10FFFF support. (now available only on including '-h' option)
- [ ] VT100 SGR (coloring) disable-mode support.
- [ ] ./configure script support.


## Usage sample

See <language>/USAGE file.

## Notes

README Updated on:2022-10-18
