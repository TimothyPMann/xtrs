#!/bin/csh -f
# Try cassette.sh instead if you do not have /bin/csh.

set done = 0
set control_file = '.cassette.ctl'
set default_format = '1'
set format_name = ( 'cas' 'cpt' 'wav' 'direct' 'debug' )

if(! -e $control_file) then
	echo "Creating" $control_file
	echo "cassette.$format_name[$default_format] 0 $default_format" \
	    > $control_file
endif

while($done != 1)
	set control = `cat $control_file`
	set filename = $control[1]
	set position = $control[2]
	if (${#control} < 3) then
	    set format = $default_format
	else
	    set format = $control[3]
	endif
	if ({ test -e $filename }) then
	    set isnew = ""
	else
	    set isnew = " (new)"
	endif
	echo ""
	echo "Tape loaded: " $filename$isnew
	echo "Type:        " $format_name[$format]
	echo "Position:    " $position
	echo ""
	echo -n "Command: "
	set command = "$<"
	set command = ( $command )

	if($#command < 1) then
	    set command = "help"
	endif
	switch($command[1])

	    case "pos":
		breaksw
	
	    case "load":
	    case "file":
		if($#command != 2) then
		    echo "Must specify a file name"
		else
		    switch($command[2])
			case *.cas:
			case *.bin:
			    set format = 1
			    breaksw
			case *.cpt:
			    set format = 2
			    breaksw
			case *.wav:
			    set format = 3
			    breaksw
			case "/dev/dsp*":
			    set format = 4
			    breaksw
			case *.debug:
			    set format = 5
			    breaksw
			default:
			    set format = $default_format
			    breaksw
		    endsw
		    echo $command[2] 0 $format > $control_file
		endif
		breaksw

	    case "type":
		if($#command != 2) then
		    echo "Types are:"
		    echo "  " $format_name
		else
		    switch($command[2])
			case "cas":
			    set format = 1
			    breaksw
			case "cpt":
			    set format = 2
			    breaksw
			case "wav":
			    set format = 3
			    breaksw
			case "direct":
			    set format = 4
			    set filename = "/dev/dsp"
			    set position = 0
			    breaksw
			case "debug":
			    set format = 5
			    breaksw
			default:
			    echo "Types are:"
			    echo "  " $format_name
			    breaksw
		    endsw
		    echo $filename $position $format > $control_file
		endif
		breaksw

	    case "rew":
		if($#command == 2) then
		    @ position = $command[2]
		else
		    @ position = 0
		endif

		echo $filename $position $format > $control_file
		breaksw

	    case "ff":
		if($#command == 2) then
		    @ position = $command[2]
		else
		    set wcout = `wc -c $filename`
		    @ position = $wcout[1]
		endif

		echo $filename $position $format > $control_file
		breaksw

	    case "quit":
	    case "exit":
	    case "done":
		set done = 1
		breaksw

	    case "help":
	    default:
		echo "Commands are:"
		echo "  pos"
		echo "  load filename"
		echo "  type {$format_name}"
		echo "  rew [position]"
		echo "  ff [position]"
		echo "  quit"
		breaksw

	endsw
end

exit 0
