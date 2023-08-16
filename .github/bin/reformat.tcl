#!/usr/bin/env tclsh

proc reformat {tclcode {pad 4}} {
  set lines [split $tclcode \n]
  set out ""
  set continued no
  set oddquotes 0
  set line [lindex $lines 0]
  set indent [expr {([string length $line]-[string length [string trimleft $line \ \t]])/$pad}]
  set pad [string repeat " " $pad]
  set lineCount [llength $lines]
  set lineIndex 0
  foreach orig $lines {
    set newline [string trim $orig \ \t]
    set line [string repeat $pad $indent]$newline
    if {[string index $line end] eq "\\"} {
      if {!$continued} {
        incr indent 2
        set continued yes
      }
    } elseif {$continued} {
      incr indent -2
      set continued no
    }
    
    if { ! [regexp {^[ \t]*\#} $line] } {
      
      # oddquotes contains : 0 when quotes are balanced
      # and 1 when they are not
      set oddquotes [expr {([count $line \"] + $oddquotes) % 2}]
      if {! $oddquotes} {
        set  nbbraces  [count $line \{]
        incr nbbraces -[count $line \}]
        set brace   [string equal [string index $newline end] \{]
        set unbrace [string equal [string index $newline 0] \}]
        if {$nbbraces>0 || $brace} {
          incr indent $nbbraces ;# [GWM] 010409 multiple open braces
        }
        if {$nbbraces<0 || $unbrace} {
          incr indent $nbbraces ;# [GWM] 010409 multiple close braces
          if {$indent<0} {
            error "unbalanced braces"
          }
          ## was: set line [string range $line [string length $pad] end]
          # 010409 remove multiple brace indentations. Including case
          set np [expr {$unbrace? [string length $pad]:-$nbbraces*[string length $pad]}]
          set line [string range $line $np end]
        }
      } else {
        # unbalanced quotes, preserve original indentation
        set line $orig
      }
    }
    append out $line
    if {$lineIndex < [expr $lineCount -1]} {
      append out "\n"
    }
    incr lineIndex
  }
  return $out
}

proc eol {} {
  switch -- $::tcl_platform(platform) {
    windows {return \r\n}
    unix {return \n}
    macintosh {return \r}
    default {error "no such platform: $::tc_platform(platform)"}
  }
}

proc count {string char} {
  set count 0
  while {[set idx [string first $char $string]]>=0} {
    set backslashes 0
    set nidx $idx
    while {[string equal [string index $string [incr nidx -1]] \\]} {
      incr backslashes
    }
    if {$backslashes % 2 == 0} {
      incr count
    }
    set string [string range $string [incr idx] end]
  }
  return $count
}

set usage "reformat.tcl ?-indent number? filename"

if {[llength $argv]!=0} {
  if {[lindex $argv 0] eq "-indent"} {
    set indent [lindex $argv 1]
    set argv [lrange $argv 2 end]
  } else  {
    set indent 4
  }
  if {[llength $argv]>1} {
    error $usage
  }
  set f [open $argv r]
  set data [read $f]
  close $f
  set permissions [file attributes $argv -permissions]
  
  set filename "$argv.tmp"
  set f [open $filename  w]
  
  puts -nonewline $f [reformat [string map [list [eol] \n] $data] $indent]
  close $f
  file copy -force $filename  $argv
  file delete -force $filename
  file attributes $argv -permissions $permissions
}


