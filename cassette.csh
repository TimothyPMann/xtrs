#!/bin/csh -f

set done = 0
set control_file = 'TRS-CASSETTE-CONTROL'

if(! -e $control_file) then
	echo "Creating" $control_file
	echo "/dev/null 0" > $control_file
endif

while($done != 1)
	set control = `cat $control_file`
	echo ""
	echo "Tape loaded: " $control[1]
	echo "Position:    " $control[2]
	echo ""
	echo -n "Command: "
	set command = "$<"
	set command = ( $command )

	if($#command < 1) then
		set done = 1
	else

	    switch($command[1])

		case "pos":
		    breaksw
	
		case "load":
		    if($#command != 2) then
			echo "Must specify a file name."
		    else
			echo $command[2] 0 > $control_file
		    endif
		    breaksw

		case "rew":
		case "ff":
		    set position = 0
		    if($#command == 2) then
			@ position = $command[2]
		    endif

		    echo $control[1] $position > $control_file
		    breaksw

		case "done":
		case "quit":
		    set done = 1
		    breaksw

		default:
		    echo "Commands are:"
		    echo "  pos"
		    echo "  load filename"
		    echo "  rew [position]"
		    echo "  ff [position]"
		    echo "  quit"
		    breaksw

	    endsw
	endif
end

exit 0
