#!/usr/bin/env tclsh
# ------------------------------------------
#
# Raptor Design Suite Power extraction utility
# !!! Temporary script invoked by Raptor !!!
# !!! Do not use directly has it is going to be replaced !!!
# !!! by a more permanent solution !!! 
#
# ------------------------------------------

# get_power_data.tcl --netlist=<file.v> --sdc=<file.sdc> 


regexp {\-\-netlist=([a-zA-Z0-9_/\.]+)} $argv tmp netlist_file
regexp {\-\-sdc=([a-zA-Z0-9_/\.]+)} $argv tmp sdc


proc hex_count_1s {length hex} {

  set ones 0

  for {set i 0} {$i<[string length $hex]} {incr i} {
    switch [string index $hex $i] {
      0 {}
      1 {incr ones 1}
      2 {incr ones 1}
      3 {incr ones 2}
      4 {incr ones 1}
      5 {incr ones 2}
      6 {incr ones 2}
      7 {incr ones 3}
      8 {incr ones 1}
      9 {incr ones 2}
      a {incr ones 2}
      b {incr ones 3}
      c {incr ones 2}
      d {incr ones 3}
      e {incr ones 3}
      f {incr ones 4}
    }
  }

  #puts "\nLength: $length"
  #puts "Hex: $hex"
  #puts "Ones: $ones"

  if {($ones > [expr $length*.45]) && ($ones < [expr $length*.55]) && ($length>16)} {
    return Very_High
  } elseif {($ones > [expr $length*.35]) && ($ones < [expr $length*.75])&& ($length>16)} {
    return High
  } else {
    return Typical
  }
}

proc find_bus_width {line} {
  set last ""
  set width 0
  if {[string first $line "\("]>0} {
    set line [string range [expr [string first $line "\("]+1] [expr [string first $line "\)"]-1]]
  }
  foreach test [split $line ,] {
    set temp [string trim $test]
    if {[string first \: $temp]>0} {
      # Remove square brackets
      while {[string first \[ $temp]>=0} {
        set temp "[string range $temp 0 [expr [string first \[ $temp]-1]] [string range $temp [expr [string first \[ $temp]+1] end]" 
      }
      while {[string first \] $temp]>=0} {
        set temp "[string range $temp 0 [expr [string first \] $temp]-1]] [string range $temp [expr [string first \] $temp]+1] end]" 
      }
      set colon [string first \: $temp]
      set start $colon
      while {[string index $temp $start] != " "} {
        incr start -1
      }
      incr start
      set endpoint $colon
          while {[string index $temp $endpoint] != " "} {
        incr endpoint
      }
      incr endpoint -1
      incr width [expr ([string range $temp $start [expr $colon-1]])-([string range $temp [expr $colon+1] $endpoint])+1]
    } elseif {![regexp {.+['].+} $temp] && ($temp != "") && ($test != $last)} {
      incr width
      set last $test
    }
  }
  return $width
}

if {![info exists netlist_file]} {
  if {[llength [glob -nocomplain *_post_synth.v]] == 1} {
    set netlist_file [glob *_post_synth.v]
  } elseif {[llength [glob -nocomplain synth_1/synthesis/*_post_synth.v]] == 1} {
    set netlist_file [glob synth_1/synthesis/*_post_synth.v]
  } elseif {[llength [glob -nocomplain *.runs/run_1/synth_1/synthesis/*_post_synth.v]] == 1} {
    set netlist_file [glob *.runs/run_1/synth_1/synthesis/*_post_synth.v]
  } else {
    puts "Error: Could not locate post-synth netlist file"
    return
  }
}

if {![info exists sdc]} {
  if {[llength [glob -nocomplain *_openfpga.sdc]] == 1} {
    set sdc [glob *_openfpga.sdc]
  } elseif {[llength [glob -nocomplain impl_1/packing/*_openfpga.sdc]] == 1} {
    set sdc [glob impl_1/packing/*_openfpga.sdc]
  } elseif {[llength [glob -nocomplain *.runs/run_1/impl_1/packing/*_openfpga.sdc]] == 1} {
    set sdc [glob *.runs/run_1/impl_1/packing/*_openfpga.sdc]
  } else {
    set sdc ""
    puts "Warning: Could not locate SDC file, timing will not be processed"
  }
}

puts "INFO: PWR: Netlist:    $netlist_file"
puts "INFO: PWR: Constraint: $sdc"

set csv_file power.csv

set clocks {}
set ios {}
set luts 0
set ffs 0
set dsps 0
set brams 0

set lut_clk_not_found ""

set parse_file [open $netlist_file r]
while {[gets $parse_file line] >= 0} {

  # puts -nonewline "."
  # puts "Debug: $line"

  set line [string trim $line]

  # Look for FF clocks
  if {[regexp {^dffre[ ].+} $line]} {
    set clock_not_found 1
    set clock ""
    set driver ""
    set source ""
    set enable ""
    set reset ""
    set instance [string range $line [expr [string first " " $line]+1] [expr [string first "(" $line]-2]]
    incr ffs
    while {![regexp {.+[;]$} $line] && ([gets $parse_file line]>=0)} {
      set line [string trim $line]
      if {[regexp {^[.]C[(].+} $line]} {
        set clock_not_found 0
        set clock [string range $line [expr [string first "(" $line]+1] [expr [string first ")" $line]-1]]
        #puts "\nClock: $clock"
        incr clock_ffs($clock)
	      lappend clocks $clock
      } elseif {[regexp {^[.]Q[(].+} $line]} {
        set driver [string trim [string range $line [expr [string first "(" $line]+1] [expr [string first ")" $line]-1]]]
	        
        if {[string index $driver 0] == "\\"} {
          set driver [string range $driver 1 end]
        }
        if {[string index $driver end] == "\]"} {
          for {set i [expr [string length $driver]]} {$i>0} {incr i -1} {
            if {[string index $driver $i] == "\["} {
              set j $i
              set i 0
            }
          }
          set driver [string trim [string range $driver 0 [expr $j-1]]]
        }
      } elseif {[regexp {^[.][DER][(].+} $line]} {
        set src_temp [string trim [string range $line [expr [string first "(" $line]+1] [expr [string first ")" $line]-1]]]

        if {[string index $src_temp 0] == "\\"} {
          set src_temp [string range $src_temp 1 end]
        }
        if {[string index $src_temp end] == "\]"} {
          for {set i [expr [string length $src_temp]]} {$i>0} {incr i -1} {
            if {[string index $src_temp $i] == "\["} {
              set j $i
              set i 0
            }
          }
          set src_temp [string trim [string range $src_temp 0 [expr $j-1]]]
        }
	#puts "source - [string trim $src_temp] : $line"
	if {![regexp {.+['].+} $src_temp]} {
	  lappend source [string trim $src_temp]
        }
      }
    }

    if {$clock_not_found} {
      puts"\nError: Clock not found for dffre instance $instance"
    } elseif {$clock != ""} {
      lappend clk_driver($clock) $driver
      #puts "source - $source"
      foreach src $source {
        lappend clk_source($clock) $src
      }
    }
  }


  # Look for DSP clocks
  if {[regexp {^RS_DSP_MULT_REGIN[ ].+} $line]} {
    set a 20
    set b 18
    incr dsps
    set dsp_module [string range $line 0 [string first " " $line]]
    set line [gets $parse_file line]
    set line [string trim [gets $parse_file line]]
    set dsp_instance [string range $line [expr [string first ")" $line]+2] [expr [string first "(" $line]-2]]
    set clock ""
    set clock_not_found 1
    set dsp_type inreg
    while {![regexp {.+[;]$} $line] && ([gets $parse_file line]>=0)} {
      set line [string trim $line]
      if {[regexp {^[.]clk[(].+} $line]} {
        set clock_not_found 0
        set clock [string range $line [expr [string first "(" $line]+1] [expr [string first ")" $line]-1]]
      } elseif {[regexp {^[.]z[(].+} $line]} {
        foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
          set temp1 [string trim $temp]
          if {[string index $temp1 0] == "\\"} {
            set temp1 [string range $temp1 1 end]
          }

          if {[string index $temp1 end] == "\]"} {
            for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
              if {[string index $temp1 $i] == "\["} {
                set j $i
                set i 0
              }
            }
            set temp1 [string range $temp1 0 [expr $j-1]]
          }
	  lappend clk_driver($clock) [string trim $temp1]
        }
      } elseif {[regexp {^[.][ab][(].+} $line]} {
        if {[regexp {^[.]a[(].+} $line]} {
          set a [find_bus_width $line]
        } else {
          set b [find_bus_width $line]
        }
        foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
          set temp1 [string trim $temp]
          if {[string index $temp1 0] == "\\"} {
            set temp1 [string range $temp1 1 end]
          }

          if {[string index $temp1 end] == "\]"} {
            for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
              if {[string index $temp1 $i] == "\["} {
                set j $i
                set i 0
              }
            }
            set temp1 [string range $temp1 0 [expr $j-1]]
          }
	  if {![regexp {.+['].+} $temp1]} {
	    lappend clk_source($clock) [string trim $temp1]
          }
        }
      }
    }

    if {($clock != "") && ($dsp_type == "inreg")} {
      incr dsp_inreg($clock,$a,$b)
    } else {
      puts "Error: Unknown DSP type: $dsp_module"
    }
  }

  # Look for BRAM clocks
  if {[regexp {^RS_TDP36K[ ].+} $line]} {
    incr brams
    set clocka_not_found 1
    set clockb_not_found 1
    set bram_clka ""
    set bram_clkb ""
    set ena .5
    set enb .5
    set wena .5
    set wenb .5
    set a_wwidth 0
    set a_rwidth 0
    set b_wwidth 0
    set b_rwidth 0
    while {![regexp {.+[;]$} $line] && ([gets $parse_file line]>=0)} {
      set line [string trim $line]
      if {[regexp {^[.]CLK_A1[(].+} $line]} {
        set clka_temp [string range $line [expr [string first "(" $line]+1] [expr [string first ")" $line]-1]]
	if {$clka_temp == "1'h0"} {
          set clka ""
        } else {
	  set clka $clka_temp
        }
        set clocka_not_found 0
      } elseif {[regexp {^[.]CLK_B1[(].+} $line]} {
        set clkb_temp [string range $line [expr [string first "(" $line]+1] [expr [string first ")" $line]-1]]
	if {$clkb_temp == "1'h0"} {
          set clkb ""
        } else {
	  set clkb $clkb_temp
        }
        set clockb_not_found 0
      } elseif {[regexp {^[.]RDATA_A.[(].+} $line]} {
        incr a_rwidth [find_bus_width $line]
        foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
          set temp1 [string trim $temp]
          if {[string index $temp1 0] == "\\"} {
            set temp1 [string range $temp1 1 end]
          }

          if {[string index $temp1 end] == "\]"} {
            for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
              if {[string index $temp1 $i] == "\["} {
                set j $i
                set i 0
              }
            }
            set temp1 [string range $temp1 0 [expr $j-1]]
          }
	  lappend clk_driver($clka) [string trim $temp1]
        }
      } elseif {[regexp {^[.]RDATA_B.[(].+} $line]} {
        incr b_rwidth [find_bus_width $line]
        foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
          set temp1 [string trim $temp]
          if {[string index $temp1 0] == "\\"} {
            set temp1 [string range $temp1 1 end]
          }
          
          if {[string index $temp1 end] == "\]"} {
            for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
              if {[string index $temp1 $i] == "\["} {
                set j $i
                set i 0
              }
            }
            set temp1 [string range $temp1 0 [expr $j-1]]
          }
	  lappend clk_driver($clkb) [string trim $temp1]
        }
      } elseif {[regexp {^[.]WDATA_A.[(].+} $line]} {
        incr a_wwidth [find_bus_width $line]
        foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
          set temp1 [string trim $temp]
          if {[string index $temp1 0] == "\\"} {
            set temp1 [string range $temp1 1 end]
          }

          if {[string index $temp1 end] == "\]"} {
            for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
              if {[string index $temp1 $i] == "\["} {
                set j $i
                set i 0
              }
            }
            set temp1 [string range $temp1 0 [expr $j-1]]
          }
          if {![regexp {.+['].+} $temp1]} {
            lappend bram_clka [string trim $temp1]
          }
        }
      } elseif {[regexp {^[.]WDATA_B.[(].+} $line]} {
        incr b_wwidth [find_bus_width $line]
        foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
          set temp1 [string trim $temp]
          if {[string index $temp1 0] == "\\"} {
            set temp1 [string range $temp1 1 end]
          }

          if {[string index $temp1 end] == "\]"} {
            for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
              if {[string index $temp1 $i] == "\["} {
                set j $i
                set i 0
              }
            }
            set temp1 [string range $temp1 0 [expr $j-1]]
          }
          if {![regexp {.+['].+} $temp1]} {
            lappend bram_clkb [string trim $temp1]
          }
        }
      } elseif {[regexp {^[.]ADDR_A.[(].+} $line] || [regexp {^[.]BE_A.[(].+} $line] || [regexp {^[.][RW]EN_A.[(].+} $line]} {
        foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
          set temp1 [string trim $temp]
          if {$temp1 == "1'h1"} {
            if {[regexp {^[.]REN_A.[(].+} $line]} {
              set ena 1
            } elseif {[regexp {^[.]WEN_A.[(].+} $line]} {
              set wena 1
            }
          } elseif {$temp1 == "1'h0"} {
            if {[regexp {^[.]REN_A.[(].+} $line]} {
              set ena 0
            } elseif {[regexp {^[.]WEN_A.[(].+} $line]} {
              set wena 0
            }
          } else {

            if {[string index $temp1 0] == "\\"} {
              set temp1 [string range $temp1 1 end]
            }
  
            if {[string index $temp1 end] == "\]"} {
              for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
                if {[string index $temp1 $i] == "\["} {
                  set j $i
                  set i 0
                }
              }
              set temp1 [string range $temp1 0 [expr $j-1]]
            }
            if {![regexp {.+['].+} $temp1]} {
              lappend bram_clka [string trim $temp1]
            }
          }
        }
      } elseif {[regexp {^[.]ADDR_B.[(].+} $line] || [regexp {^[.]BE_B.[(].+} $line] || [regexp {^[.][RW]EN_B.[(].+} $line]} {
        foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
          set temp1 [string trim $temp]
	  if {$temp1 == "1'h1"} {
            if {[regexp {^[.]REN_B.[(].+} $line]} {
              set enb 1
	    } elseif {[regexp {^[.]WEN_B.[(].+} $line]} {
              set wenb 1
	    }
	  } elseif {$temp1 == "1'h0"} {
            if {[regexp {^[.]REN_B.[(].+} $line]} {
              set enb 0
	    } elseif {[regexp {^[.]WEN_B.[(].+} $line]} {
              set wenb 0
	    }
	  } else {

            if {[string index $temp1 0] == "\\"} {
              set temp1 [string range $temp1 1 end]
            }

            if {[string index $temp1 end] == "\]"} {
              for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
                if {[string index $temp1 $i] == "\["} {
                  set j $i
                  set i 0
                }
              }
              if {!$clockb_not_found} {
                set temp1 [string range $temp1 0 [expr $j-1]]
              }
            }
	    if {![regexp {.+['].+} $temp1]} {
	      lappend bram_clkb [string trim $temp1]
            }
          }
        }
      }
    }
    if {!($clocka_not_found || $clockb_not_found)} {
      puts "a_wwidth=$a_wwidth a_rwidth=$a_rwidth b_wwidth=$b_wwidth b_rwidth=$b_rwidth"
      if {$a_wwidth > $a_rwidth} {
        set a_width $a_wwidth
      } else {
        set a_width $a_rwidth
      }
      if {$b_wwidth > $b_rwidth} {
        set b_width $b_wwidth
      } else {
        set b_width $b_rwidth
      }
      incr clock_bram($clka,$clkb,$ena,$enb,$wena,$wenb,$a_width,$b_width)
      foreach temp $bram_clka {
        lappend clk_source($clka) $temp
      }
      foreach temp $bram_clkb {
        lappend clk_source($clkb) $temp
      }
    } else {
      puts "Error: BRAM clock not found"
    }
  }
}
close $parse_file

set clocks [lsort -unique $clocks]
foreach clk $clocks {
  set sdc_freq($clk) ""
}

foreach test_clk $clocks {
  set clk_driver($test_clk) [lsort -unique $clk_driver($test_clk)]
  set clk_source($test_clk) [lsort -unique $clk_source($test_clk)]
  #puts "[llength $clk_driver($test_clk)]: clk_driver($test_clk): $clk_driver($test_clk)"
  #puts "[llength $clk_source($test_clk)]: clk_source($test_clk): $clk_source($test_clk)"
}  

proc get_ports { args } {
    set args [decode $args]
    return $args
}

proc create_clock { args } {
    global sdc_freq
    # Get the clock name 
    for {set i 0} {$i < [llength $args]} {incr i} {
        set arg [lindex $args $i]
        if {$arg == "-name"} {
            incr i
            set arg [lindex $args $i]
            set sdc_clk [decode $arg]
        } elseif {$arg == "-period"} {
            incr i
        } else {
            set sdc_clk  [decode $arg]
        }
    }
    # get the period
    for {set i 0} {$i < [llength $args]} {incr i} {
        set arg [lindex $args $i]
        if {$arg == "-period"} {
            incr i
            set arg [lindex $args $i]
            set sdc_freq($sdc_clk) [format %3.3f [expr 1000/$arg]]
        } 
    }
}

proc decode { args } {
    regsub -all {@\*@} $args "{*}" args
    regsub -all {@} $args "\[" args
    regsub -all {%} $args "\]" args
    return $args
}

proc get_clocks { args } {
    set args [decode $args]
    return $args
}

proc set_clock_groups { args } {
}

proc set_false_path { args } {
}

proc sdc_tcl_friendly { file } {
    set fid [open $file "r"]
    set orig [read $fid]
    regsub -all {\[\*\]} $orig "@*@" orig
    regsub -all {\{\*\}} $orig "@*@" orig
    close $fid
    set text ""
    set c [string index $orig 0]
    for {set i 1} {$i < [string length $orig]} {incr i} {
        append text $c
        set c [string index $orig $i]
        if {$c == "\["} {
            incr i
            set c [string index $orig $i]
            if [string is digit $c] {
                append text "@"
                while {$c != "\]"} {
                    append text $c
                    incr i
                    set c [string index $orig $i]
                }
            incr i
            set c [string index $orig $i]
            append text "%"
        } else {
          append text "\["
        }
      }
    }
   return $text
}


if { $sdc != ""} {
    set translated [sdc_tcl_friendly $sdc]    
    set sdc_source_result ""
    catch {eval $translated} sdc_source_result
    if {$sdc_source_result != ""} {
        puts "WARNING: Some of SDC content could not be processed:\n$sdc_source_result" 
    }
}



# Initialize LUT clock sigs
foreach test_clk $clocks {
  set sync_lut_sigs($test_clk) ""
}

# Find Clocking for LUTs
set parse_file [open $netlist_file r]
while {[gets $parse_file line] >= 0} {
  set line [string trim $line]

  # Look for LUTs
  if {[regexp {^.[$]lut[ ].+} $line]} {
    incr luts
    set clk_not_found 1
    set glitch "Unknown"
    set instance "Unknown"
    set lut_clk_sigs ""

    while {![regexp {.+[;]$} $line] && ([gets $parse_file line]>=0)}  {
      set line [string trim $line]
      if {[regexp {^[.]LUT[(].+} $line]} {
        set length [string range $line [expr [string first "(" $line]+1] [expr [string first "'" $line]-1]]
        set hex [string range $line [expr [string first "h" $line]+1] [expr [string first ")" $line]-1]]
        set glitch [hex_count_1s $length $hex]
      } elseif {[string range $line 0 1]== "\) "} {
        set instance [string range $line 2 [expr [string length $line]-3]]
      } elseif {[regexp {^[.]A[(].+} $line]} {
        foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
          set temp1 [string trim $temp]
          if {[string index $temp1 0] == "\\"} {
            set temp1 [string range $temp1 1 end]
          }
	  if {[string index $temp1 end] == "\]"} {
            for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
              if {[string index $temp1 $i] == "\["} {
                set j $i
	        set i 0
              }
            }
	    set temp1 [string range $temp1 0 [expr $j-1]]
          }
	  set temp1 [string trim $temp1]
          foreach test_clk $clocks {
      
            foreach test_sig $clk_driver($test_clk) {
              if {($temp1 == $test_sig) && $clk_not_found} {
                incr lut_clk($test_clk,$glitch)
                set clk_not_found 0
		set lut_clk_temp $test_clk
              }
            }
          }
	  if {$clk_not_found} {
            foreach test_clk $clocks {
              foreach test_sig $clk_source($test_clk) {
                if {($temp1 == $test_sig) && $clk_not_found} {
                  incr lut_clk($test_clk,$glitch)
                  set clk_not_found 0
		set lut_clk_temp $test_clk
                }
              }
            }
          }
	  lappend lut_clk_sigs $temp1
        }
      } elseif {[regexp {^[.]Y[(].+} $line]} {
        set temp1 [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]]
        if {[string index $temp1 0] == "\\"} {
          set temp1 [string range $temp1 1 end]
        }

        if {[string index $temp1 end] == "\]"} {
          for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
            if {[string index $temp1 $i] == "\["} {
              set j $i
              set i 0
            }
          }
          set temp1 [string range $temp1 0 [expr $j-1]]
        }
	set temp1 [string trim $temp1]
        if {$clk_not_found} {
          foreach test_clk $clocks {
            foreach test_sig $clk_source($test_clk) {
              if {($temp1 == $test_sig) && $clk_not_found} {
                incr lut_clk($test_clk,$glitch)
                set clk_not_found 0
		set lut_clk_temp $test_clk
              }
            }
          }
        }
	lappend lut_clk_sigs $temp1
      }
    }
    if {$clk_not_found} {
      #puts "Clock not found for instance, $instance"
      lappend lut_clk_not_found $instance
    } else {
      foreach temp $lut_clk_sigs {
        lappend sync_lut_sigs($lut_clk_temp) $temp
        lappend clk_source($lut_clk_temp) $temp
      }
    }
  }
}
close $parse_file

if {[llength $lut_clk_not_found] > 0} {
  set iterations 5
} else {
  set iterations 0
}
for {set k 0} {$k<$iterations} {incr k} {
  puts "\n**** LUT iteration $k : [llength $lut_clk_not_found] LUTs without clock ****"
  set lut_clk_not_found_temp $lut_clk_not_found
  set lut_clk_not_found ""
  foreach clock $clocks {
    set sync_lut_sigs_temp($clock) [lsort -unique $sync_lut_sigs($clock)]
    set sync_lut_sigs($clock) ""
  }
  set glitch_line_temp ""
  set lut_index 0
  set parse_file [open $netlist_file r]
  while {($lut_index < [llength $lut_clk_not_found]) || ([gets $parse_file line] >= 0)} {
    set line [string trim $line]
    if {[string range $line 2 [expr [string length $line]-3]]==[lindex $lut_clk_not_found_temp $lut_index]} {
      set clk_not_found 1
      set lut_clk_sigs ""
      while {![regexp {.+[;]$} $line] && ([gets $parse_file line]>=0)} {
        set line [string trim $line]
        if {[regexp {^[.][AY][(].+} $line]} {
          foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
            set temp1 [string trim $temp]
            if {[string index $temp1 0] == "\\"} {
              set temp1 [string range $temp1 1 end]
            }
            if {[string index $temp1 end] == "\]"} {
              for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
                if {[string index $temp1 $i] == "\["} {
                  set j $i
	          set i 0
                }
              }
              set temp1 [string range $temp1 0 [expr $j-1]]
            }
            set temp1 [string trim $temp1]
            foreach test_clk $clocks {
              foreach test_sig $sync_lut_sigs_temp($test_clk) {
                if {($temp1 == $test_sig) && $clk_not_found} {
                  set length [string range $glitch_line [expr [string first "(" $glitch_line]+1] [expr [string first "'" $glitch_line]-1]]
                  set hex [string range $glitch_line [expr [string first "h" $glitch_line]+1] [expr [string first ")" $glitch_line]-1]]
                  set glitch [hex_count_1s $length $hex]

                  incr lut_clk($test_clk,$glitch)
                  set clk_not_found 0
                  set lut_clk_temp $test_clk
                }
              }
            }
            lappend lut_clk_sigs $temp1
          }
        }
      }
      if {$clk_not_found && ($k<[expr $iterations-1])} {
        #puts "Clock not found for instance, [lindex $lut_clk_not_found_temp $lut_index]"
        #puts -nonewline "[llength $lut_clk_not_found] "
        #puts -nonewline "x"
        lappend lut_clk_not_found [lindex $lut_clk_not_found_temp $lut_index]
      } elseif {$clk_not_found} {
	#puts "glitch_line $glitch_line"
        #set length [string range $glitch_line [expr [string first "(" $glitch_line]+1] [expr [string first "'" $glitch_line]-1]]
        #set hex [string range $glitch_line [expr [string first "h" $glitch_line]+1] [expr [string first ")" $glitch_line]-1]]
	#puts "length=$length, hex=$hex"
        set glitch [hex_count_1s $length $hex]

        incr lut_clk(Unknown,Typical)

      } else {
        #puts -nonewline "."
        set sync_lut_sigs($lut_clk_temp) "$sync_lut_sigs($lut_clk_temp) $lut_clk_sigs"
        set clk_source($lut_clk_temp) "$clk_source($lut_clk_temp) $lut_clk_sigs"
      }
      incr lut_index
    }
    set glitch_line $glitch_line_temp
    set glitch_line_temp $line
  }
  close $parse_file
  set lut_clk_not_found_temp $lut_clk_not_found
  set lut_clk_not_found ""
  foreach temp $lut_clk_not_found_temp {
    if {$temp != ""} {
      lappend lut_clk_not_found $temp
    }
  }
  if {[llength $lut_clk_not_found] == 0} {
    set k 5
  }
}

foreach test_clk $clocks {
  set clk_driver($test_clk) [lsort -unique $clk_driver($test_clk)]
  set clk_source($test_clk) [lsort -unique $clk_source($test_clk)]
  #puts "[llength $clk_driver($test_clk)]: clk_driver($test_clk): $clk_driver($test_clk)"
  #puts "[llength $clk_source($test_clk)]: clk_source($test_clk): $clk_source($test_clk)"
}

  # Look for DSP clocks
set parse_file [open $netlist_file r]
while {[gets $parse_file line] >= 0} {
  set line [string trim $line]
  if {[regexp {^RS_DSP_MULT[ ].+} $line]} {
    set a 20
    set b 18
    incr dsps
    set dsp_module [string range $line 0 [string first " " $line]]
    set line [gets $parse_file line]
    set line [string trim [gets $parse_file line]]
    set dsp_instance [string range $line [expr [string first ")" $line]+2] [expr [string first "(" $line]-2]]
    set clk "Unknown"
    set dsp_type "async"
    set clk_not_found 1
    while {![regexp {.+[;]$} $line] && ([gets $parse_file line]>=0)} {
      set line [string trim $line]
      if {[regexp {^[.][ab][(].+} $line]} {
        if {[regexp {^[.]a[(].+} $line]} {
          set a [find_bus_width $line]
        } else {
          set b [find_bus_width $line]
        }
        foreach temp [lsort -unique [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ]] {
          set temp1 [string trim $temp]
          if {[string index $temp1 0] == "\\"} {
            set temp1 [string range $temp1 1 end]
          }

          if {[string index $temp1 end] == "\]"} {
            for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
              if {[string index $temp1 $i] == "\["} {
                set j $i
                set i 0
              }
            }
            set temp1 [string trim [string range $temp1 0 [expr $j-1]]]
          }
          if {$clk_not_found && ![regexp {.+['].+} $temp1]} {
	    foreach test_clk $clocks {
              foreach test_sig $clk_driver($test_clk) {
                if {($temp1 == $test_sig) && $clk_not_found} {
                  set clk $test_clk
                  set clk_not_found 0
                }
              }
            }
          }
          if {$clk_not_found && ![regexp {.+['].+} $temp1]} {
            foreach test_clk $clocks {
              foreach test_sig $clk_source($test_clk) {
                if {($temp1 == $test_sig) && $clk_not_found} {
                  set clk $test_clk
                  set clk_not_found 0
                }
              }
            }
          }
        }
      } elseif {[regexp {^[.]z[(].+} $line]} {
        foreach temp [split [string trim [string trim [string trim [string range $line [expr [string first \( $line]+1] [expr [string first \) $line]-1]] \{] \}]] , ] {
          set temp1 [string trim $temp]
          if {[string index $temp1 0] == "\\"} {
            set temp1 [string range $temp1 1 end]
          }

          if {[string index $temp1 end] == "\]"} {
            for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
              if {[string index $temp1 $i] == "\["} {
                set j $i
                set i 0
              }
            }
            set temp1 [string range $temp1 0 [expr $j-1]]
          }
          if {$clk_not_found} {
            foreach test_clk $clocks {
              foreach test_sig $clk_source($test_clk) {
                if {($temp1 == $test_sig) && $clk_not_found} {
                  set clk $test_clk
                  set clk_not_found 0
                }
              }
            }
          }
        }
      }
    }
    incr dsp_async($clk,$a,$b)
  }
}

# Find Clocking for I/Os

set io_clk_not_found ""

set parse_file [open $netlist_file r]
while {[gets $parse_file line] >= 0} {
  set line [string trim $line]

  # Look for Inputs
  if {[regexp {^input[ ].+} $line]} {
    set clk "Unknown"
    set clk_not_found 1
    set io_sig [string range $line [expr [string last \  $line]+1] [expr [string last \; $line]-1]]
    set clk_type "SDR"
    foreach test_clk $clocks {
      if {$io_sig == $test_clk} {
        set clk_type "Clock"
        set clk_not_found 0
        set clk $test_clk
      } else {
        foreach test_sig $clk_source($test_clk) {
          if {($io_sig == $test_sig) && $clk_not_found} {
            set clk $test_clk
            set clk_not_found 0
          }
        }
      }
    }
    lappend ios "[find_bus_width $line] $io_sig Input $clk_type $clk"
    if {$clk_not_found} {
      lappend io_clk_not_found $io_sig
    }
  }

  # Look for Outputs
  if {[regexp {^output[ ].+} $line]} {
    set clk "Unknown"
    set clk_not_found 1
    set io_sig [string range $line [expr [string last \  $line]+1] [expr [string last \; $line]-1]]
    foreach test_clk $clocks {
      foreach test_sig $clk_driver($test_clk) {
        if {($io_sig == $test_sig) && $clk_not_found} {
          set clk $test_clk
          set clk_not_found 0
        }
      }
    }
    if {$clk_not_found} {
      foreach test_clk $clocks {
        foreach test_sig $clk_source($test_clk) {
          if {($io_sig == $test_sig) && $clk_not_found} {
            set clk $test_clk
            set clk_not_found 0
          }
        }
      }
    }
    if {$clk_not_found} {
      #lappend io_clk_not_found $io_sig
      lappend io_clk_not_found "[find_bus_width $line] $io_sig Output SDR $clk"
    } else {
      lappend ios "[find_bus_width $line] $io_sig Output SDR $clk"
    }
  }

  # Look for Bidirs
  if {[regexp {^inout[ ].+} $line]} {
    set clk "Unknown"
    set clk_not_found 1
    set io_sig [string range $line [expr [string last \  $line]+1] [expr [string last \; $line]-1]]
    foreach test_clk $clocks {
      foreach test_sig $clk_driver($test_clk) {
        if {($io_sig == $test_sig) && $clk_not_found} {
          set clk $test_clk 
          set clk_not_found 0
        }
      }
    }
    if {$clk_not_found} {
      foreach test_clk $clocks {
        foreach test_sig $clk_source($test_clk) {
          if {($io_sig == $test_sig) && $clk_not_found} {
            set clk $test_clk 
            set clk_not_found 0
          }
        }
      }
    }
    if {$clk_not_found} {
      lappend io_clk_not_found "[find_bus_width $line] $io_sig Bi-dir SDR $clk"
    } else {
      lappend ios "[find_bus_width $line] $io_sig Bi-dir SDR $clk"
    }
  }
  if {[regexp {^assign[ ].+} $line]} {
    set assign_name [lindex [string range $line 0 [expr [string length $line]-2]] 1]
    if {[string index $assign_name 0] == "\\"} {
      set assign_name [string range $assign_name 1 end]
    }
    if {[string index $assign_name end] == "\]"} {
      for {set i [expr [string length $assign_name]]} {$i>0} {incr i -1} {
        if {[string index $assign_name $i] == "\["} {
          set j $i
          set i 0
          }
      }
      set assign_name [string range $assign_name 0 [expr $j-1]]
    }
    set assign_name [string trim $assign_name]
    foreach tempx $io_clk_not_found {
      set temp [lindex $tempx 1]
      if {$assign_name == $temp} {
        set clk_not_found 1
        set clock "Unknown"
        set line [string trim [string range $line [expr [string first "=" $line]+1] [expr [string length $line]-1]]]
	if {[string index $line 0] == "\{"} {
          set line [string trim [string range $line 1 [expr [string length $line]-3]]]
        }
        foreach temp1 [split $line ,] {
          set temp1 [string trim $temp1]
          if {[string index $temp1 0] == "\\"} {
            set temp1 [string range $temp1 1 end]
          }
          if {[string index $temp1 end] == "\]"} {
            for {set i [expr [string length $temp1]]} {$i>0} {incr i -1} {
              if {[string index $temp1 $i] == "\["} {
                set j $i
                set i 0
              }
            }
            set temp1 [string range $temp1 0 [expr $j-1]]
          }
          set temp1 [string trim $temp1]
          foreach test_clk $clocks {
            foreach test_sig $clk_driver($test_clk) {
              if {($temp1 == $test_sig) && $clk_not_found} {
                set clk_not_found 0
		set clock $test_clk
              }
            }
          }
          foreach test_clk $clocks {
            foreach test_sig $clk_source($test_clk) {
              if {($temp1 == $test_sig) && $clk_not_found} {
                set clk_not_found 0
                set clock $test_clk
              }
            }
          }
        }
        lappend ios "[lrange $tempx 0 3] $clock" 
      }
    }
  }
}

# Reporting

puts "\nClocks: [llength $clocks]"
foreach clock $clocks {
  puts "\t$clock $sdc_freq($clock)"
}

puts "\nI/Os: [llength $ios]"
foreach io $ios {
  puts "\t$io"
}

#if {[llength $io_clk_not_found] >0} {
  #puts "\n*** There are [llength $io_clk_not_found] Unknown clocked I/Os ***"
  #puts "$io_clk_not_found"
#}


puts "\nLUTs: $luts"
foreach lut_type [array names lut_clk] {
  puts "\t[split $lut_type ","] : $lut_clk($lut_type)"
}

if {[llength $lut_clk_not_found] > 0} {
  puts "\n*** There are [llength $lut_clk_not_found] Unknown clocked LUTs ***"
}

puts "\nFFs: $ffs"

foreach clock [array names clock_ffs] {
  puts "\t$clock : $clock_ffs($clock)"
}

if {$dsps > 0} {
  puts "\nDSPs: $dsps"
  if {[array size dsp_inreg]>0} {
    puts "\tInreg"
    foreach clock [array names dsp_inreg] {
      puts "\t\t[lindex [split $clock ,] 0] : [lindex [split $clock ,] 1] [lindex [split $clock ,] 2] : $dsp_inreg($clock)"
    }
  }
  if {[array size dsp_async]>0} {
    puts "\tAsync"
    foreach clock [array names dsp_async] {
      puts "\t\t[lindex [split $clock ,] 0] : [lindex [split $clock ,] 1] [lindex [split $clock ,] 2] : $dsp_async($clock)"
    }
  }
}

if {$brams > 0} {
  puts "\nBRAMs: $brams"
  
  foreach clock [array names clock_bram] {
    set clka [lindex [split $clock ,] 0]
    set clkb [lindex [split $clock ,] 1]
    set ena [lindex [split $clock ,] 2]
    set enb [lindex [split $clock ,] 3]
    set wea [lindex [split $clock ,] 4]
    set web [lindex [split $clock ,] 5]
    set a_width [lindex [split $clock ,] 6]
    set b_width [lindex [split $clock ,] 7]
    if {$clkb == ""} {
      puts "\t36k SP : $clka : $a_width : $ena $web : $clock_bram($clock)"
    } elseif {($enb==0 && $web!=0) && ($ena!=0 && $wea==0)} {
      puts "\t36k SDP : $clka $clkb : $a_width $b_width : $ena $enb : $wea $web : $clock_bram($clock)"
    } elseif {($ena==0 && $wea!=0) && ($enb!=0 && $web==0)} {
      puts "\t36k SDP : $clka $clkb : $a_width $b_width : $ena $enb : $wea $web : $clock_bram($clock)"
    } else {
      puts "\t36k TDP : $clka $clkb : $a_width $b_width : $ena $enb : $wea $web : $clock_bram($clock)"
    }
  }
}

set csv [open $csv_file w]

puts $csv "Clocks"

foreach clock $clocks {
  puts $csv "Enabled,,I/O,$clock,$sdc_freq($clock)"
}

puts $csv "\nFabric Logic Element"
foreach lut_type [array names lut_clk] {
  puts $csv "Enabled,,$lut_clk($lut_type),,[lindex [split $lut_type ","] 0],,[split [lindex [split $lut_type ","] 1] "_"]"
}
foreach clock [array names clock_ffs] {
  puts $csv "Enabled,,,$clock_ffs($clock),$clock"
}  

puts $csv "\nBRAM"
foreach clock [array names clock_bram] {
  set clka [lindex [split $clock ,] 0]
  set clkb [lindex [split $clock ,] 1]
  set ena [lindex [split $clock ,] 2]
  set enb [lindex [split $clock ,] 3]
  set wea [lindex [split $clock ,] 4]
  set web [lindex [split $clock ,] 5]
  if {$clkb == ""} {
    puts $csv "Enabled,,36k SP,$clock_bram($clock),$clka,36,$wea,$ena"
  } elseif {($enb==0 && $web!=0) && ($ena!=0 && $wea==0)} {
    puts $csv "Enabled,,36k SDP,$clock_bram($clock),$clkb,36,,$web,,,,,$clka,36,,$ena"
  } elseif {($ena==0 && $wea!=0) && ($enb!=0 && $web==0)} {
    puts $csv "Enabled,,36k SDP,$clock_bram($clock),$clka,36,,$wea,,,,,$clkb,36,,$enb"
  } else {
    puts $csv "Enabled,,36k TDP,$clock_bram($clock),$clka,36,$wea,$ena,,,,,$clkb,36,$web,$enb"
  }
}

puts $csv "\nDSP"
foreach clock [array names dsp_inreg] {
  puts $csv "Enabled,,$dsp_inreg($clock),Muliplier-only,[lindex [split $clock ,] 1],[lindex [split $clock ,] 2],[lindex [split $clock ,] 0],Input-only"
}
foreach clock [array names dsp_async] {
  puts $csv "Enabled,,$dsp_async($clock),Muliplier-only,[lindex [split $clock ,] 1],[lindex [split $clock ,] 2],[lindex [split $clock ,] 0],None"
}

puts $csv "\nI/O"
foreach io $ios {
  if {[lindex $io 4] == "Unknown"} {
    set enabled "Disabled"
  } else {
    set enabled "Enabled"
  }
  puts $csv "$enabled,[lindex $io 1],[lindex $io 0],[lindex $io 2],LVCMOS 1.8V (HR),2 mA,Slow,,[lindex $io 3],[lindex $io 4]"
}

close $csv

puts "\nINFO: PWR: Created [pwd]/power.csv\n"
