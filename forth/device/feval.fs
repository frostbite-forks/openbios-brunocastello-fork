\ tag: FCode evaluator
\ 
\ this code implements an fcode evaluator 
\ as described in IEEE 1275-1994
\ 
\ Copyright (C) 2003 Stefan Reinauer
\ 
\ See the file "COPYING" for further information about
\ the copyright and warranty status of this work.
\ 

defer init-fcode-table

: alloc-fcode-table 
  4096 cells alloc-mem to fcode-table
  ?fcode-verbose if
    ." fcode-table at 0x" fcode-table . cr
  then
  init-fcode-table
  ;
 
: free-fcode-table
  fcode-table 4096 cells free-mem
  0 to fcode-table
  ;

: (debug-feval) ( fcode# -- fcode# )
  \ Address
  fcode-stream 1 - . ." : "

  \ Indicate if word is compiled
  state @ 0<> if
    ." (compile) "
  then
  dup fcode>xt cell - lfa2name type
  dup ."  [ 0x" . ." ]" cr
  ;

: (feval) ( -- ?? )
  begin
    fcode#
    ?fcode-verbose if
      (debug-feval)
    then
    fcode>xt
    dup flags? 0<> state @ 0= or if
      execute
    else
      ,
    then
  fcode-end @ until

  \ If we've executed incorrect FCode we may have reached the end of the FCode
  \ program but still be in compile mode. Make sure that if this has happened
  \ then we switch back to immediate mode to prevent internal OpenBIOS errors.
  tmp-comp-depth @ -1 <> if
    -1 tmp-comp-depth !
    tmp-comp-buf @ @ here!
    0 state !
  then
;

\ byte-load auto-creates an instance context when invoked from the OF
\ prompt against a vendor PCI expansion ROM. Vendor FCode payloads
\ (e.g. the ATI Rage 128 PRO 136.rom) reference instance-specific
\ words like my-self/my-space/pci-map-in early on, so they need
\ either an existing current instance (from select-dev) or one that
\ byte-load opens itself on the active package. The opened instance
\ is closed again when feval finishes, leaving caller-visible state
\ unchanged.

: byte-load ( addr xt -- )
  ?fcode-verbose if
    cr ." byte-load: evaluating fcode at 0x" over . cr
  then

  \ save state
  >r >r fcode-push-state r> r>

  \ set fcode-c@ defer
  dup 1 = if drop ['] c@ then      \ FIXME: uses c@ rather than rb@ for now...
  to fcode-c@
  dup to fcode-stream-start
  to fcode-stream
  1 to fcode-spread
  false to ?fcode-offset16
  alloc-fcode-table
  false fcode-end !

  \ Suppress the "isn't unique" warning while evaluating vendor FCode:
  \ PCI ROMs routinely redefine display-device words like color!,
  \ set-colors and fill-rectangle that OpenBIOS pre-defines in its
  \ built-in framebuffer driver. Save and restore the previous value
  \ so nested byte-load calls leave the global state intact.
  suppress-redefine-warning? >r
  true to suppress-redefine-warning?

  \ Ensure a current instance exists for the FCode payload. If the user
  \ has done "select-dev" but my-self has been cleared (e.g. by a
  \ previous unselect-dev), or if only active-package is set, open a
  \ temporary instance on the active package. We push the ihandle of
  \ the temporary instance (or 0 if none was opened) onto R, along
  \ with the previous my-self value, so we can close-package and
  \ restore on exit.
  my-self >r                            \ R: prior-my-self
  my-self 0= active-package 0<> and if
    " " active-package open-package dup if
      dup to my-self                    \ make new instance current
    then
    \ ( ihandle-or-0 )
  else
    0                                   \ ( 0 ) — nothing to close
  then
  >r                                    \ R: prior-my-self temp-ihandle

  \ protect against stack overflow/underflow
  0 0 0 0 0 0 depth >r

  ['] (feval) catch ?dup if
    cr ." byte-load: exception caught! throw=" .
    ." stream-pos=0x" fcode-stream fcode-stream-start - . cr
    ." last fcode-stream=0x" fcode-stream . cr
  then

  s" fcode-debug?" evaluate if
    depth r@ <> if
      cr ." byte-load: warning stack overflow, diff " depth r@ - . cr
    then
  then

  r> depth! 3drop 3drop

  \ Close the temporary instance (if any), then restore my-self.
  r> ?dup if close-package then
  r> to my-self

  r> to suppress-redefine-warning?

  free-fcode-table

  \ restore state
  fcode-pop-state
;
